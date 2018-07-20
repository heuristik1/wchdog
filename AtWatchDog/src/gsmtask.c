/*
 * Copyright (c) 2018 Integrated Solutions, Inc.
 * All rights reserved, worldwide.
 *
 */


#include "stm32f0xx_gpio.h"
#include "stm32f0xx_usart.h"
#include "stm32f0xx_rcc.h"

/* Scheduler include files */
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "gsmtask.h"
#include "main.h"
#include "Debug.h"
#include "uart5.h"
#include "Modem.h"
#include "Flash.h"
#include "process.h"
#include "gsm.h"
#include "LED.h"

extern xSemaphoreHandle	connect_Semaphore;

#include "gsmconfig.h"

bool gHoldSamplesDuringModemActivity = false;

#if 1
/*	JSON format
	[{
		"IMSI": "310260969409697",
		"gps": "-112.105576,33.630493",
		"records": [{
				"ts": "20",
				"mv": "48104",
				"t": "223",
				"f": "0040"
			},
			{
				"ts": "21",
				"mv": "48104",
				"t": "223",
				"f": "0040"
			},
			{
				"ts": "22",
				"mv": "48104",
				"t": "223",
				"f": "0040"
			},
			{
				"ts": "23",
				"mv": "48104",
				"t": "223",
				"f": "0040"
			}
		]
	}]

 */

static int flush (char *json) {
	int retries = 3;
	int result;

	// !! modifies json!
	if (json[0]) {
		strcat(json, "]}]");

		while(retries--) {
			result = gsm_send_json(g_url, g_auth, json);
			if (result)
				return 1;
		}

	}
	return 0;
}

static int add_record(char *records, int sz_records, Event_t *pev, char const *long_lat, char const *imsi) {
	char single_rec[128];
	int i;
	int result = 1;	// expect OK to add to buffer

	snprintf(single_rec, sizeof(single_rec), "{\"ts\":%lu,\"mv\":%d,\"t\":%d,\"f\":\"%04x\"}",
			(unsigned long)pev->TimeStamp,
			pev->mVolts,
			pev->dTemp,
			pev->flags);

	i = strlen(records);

	// flush ?
	if (pev == NULL || i + 1 + strlen(single_rec) + 1 + 1 >= sz_records - 5) {	// 5 for slop
		if (!flush(records)) {
			result = 0; 	// result FAILED, bail
		} else {
			result = 2; 	// indicated sent successfully
		}

		// restart
		records[0] = 0;
		i = 0;
	}

	if (i == 0) {
		sprintf(records, "[{\"IMSI\":%s,\"gps\":\"%s\",\"records\": [",
				imsi,
				long_lat);

	} else {
		strcat(records, ",");	// more in list
	}
	strcat(records, single_rec);
	return (result);
}
#else
static void flush (char *json) {
	// !! modifies json!
	if (json[0]) {
		strcat(json, "]");

		gsm_send_json(g_url, g_auth, json);
	}
}

static void add_record(char *records, int sz_records, Event_t *pev, char const *long_lat) {
	char single_rec[128];
	int i;

	snprintf(single_rec, sizeof(single_rec), "{\"ts\":%lu,\"mv\":%d,\"t\":%d,\"f\":\"%04x\",\"gps\":\"%s\"}",
			(unsigned long)pev->TimeStamp,
			pev->mVolts,
			pev->dTemp,
			pev->flags,
			long_lat);

	i = strlen(records);

	// flush ?
	if (pev == NULL || i + 1 + strlen(single_rec) + 1 + 1 >= sz_records - 5) {	// 5 for slop
		flush(records);

		// restart
		records[0] = 0;
		i = 0;
	}

	if (i == 0) {
		strcpy(records, "[");	// start list
	} else {
		strcat(records, ",");	// more in list
	}
	strcat(records, single_rec);
}
#endif

void prvCellTask( void *pvParameters )
{
	Event_t ev;
	static char long_lat[128];
	static char IMSI[32];
	static char temp_long_lat[128];
	uint32 t=0;
	uint32 temp_t=0;
	int send_result=0;

	debug_Printf("gsmtask.c configuring for \"%s\", APN \"%s\"\n", g_name, g_apn);
	gsm_set_apn(g_apn);

	// AT init and get time
	do{
		at_wake();
		LED_OFF(GREEN_LED);
		LED_ON(RED_LED);
		vTaskDelay(MS_TO_TICKS(500));
		gsm_get_location_and_time (long_lat, sizeof(long_lat), &t);
		LED_ON(GREEN_LED);
		LED_OFF(RED_LED);
		vTaskDelay(MS_TO_TICKS(500));
	}while(t==0);

	LED_OFF(RED_LED);		// red off if talking to modem
	LED_ON(GREEN_LED);		// solid on after connect
	Time_Callback(t);		// set the time for the flash logger
	//gsm_get_IMEI (IMSI, sizeof(IMSI));
	gsm_get_IMSI (IMSI, sizeof(IMSI));

	at_sleep();

	for( ;; )
	{
		// wait for the semaphore from the main task
		if ( xSemaphoreTake(connect_Semaphore,10000+MS_TO_TICKS(Config.Report_Rate * 100)) == pdTRUE )
		{
			at_wake();

#if defined(USING_ISITEST02)
			//static char json[128];

			gsm_get_location_and_time (long_lat, sizeof(long_lat), &t);

			//debug_Printf("gsmtask.c: ccipgsmloc \"%s\"\n", gsm_get_cipgsmloc());
			//snprintf(json, sizeof(json), "[{\"cipgsmloc\":\"%s\"}]", gsm_get_cipgsmloc());
			//gsm_send_json(g_url, g_auth, json);
#else
			int sz_records = 1024;
			char *records = malloc(sz_records);

			if (!records) {
				debug_Puts("gsmtask.c malloc() failed\n");
			} else {
				records[0] = 0;

				LED_OFF(GREEN_LED);
				LED_ON(BLUE_LED);
				debug_Puts("--- Sending ---");

				// AT init and get time - force to be active again
				do{
					vTaskDelay(MS_TO_TICKS(500));
					gsm_get_location_and_time (long_lat, sizeof(long_lat), &t);
				}while(t==0);

				// saw condition where it failed so check return first and use temporary vars
				if (gsm_get_location_and_time (temp_long_lat, sizeof(temp_long_lat), &temp_t) != -1)
				{
					strcpy(long_lat,temp_long_lat);
					t = temp_t;
					Time_Callback(t);		// always sync time
				}

				debug_Printf("gsmtask.c long_lat: '%s'\n", long_lat);
				debug_Printf("gsmtask.c time: '%u'\n", t);

				// do the work to upload
	//			gsm_GetConfig();				// not needed right now

				send_result = 0;
				Begin_Report();					// have flash prepare the reports

				while (Report_Callback(&ev) != -1)
				{
					/*
					uint32	TimeStamp;			//	Seconds since 1970
					uint16	mVolts;				//	0.001 V
					uint16	dTemp;				//	0.1 C
					uint16	flags;
					uint16	filler;
					 */
					debug_Printf("ts:%d, v:%d, t:%d, f:%04x gps:%s\r\n", ev.TimeStamp,
																  ev.mVolts,
																  ev.dTemp,
																  ev.flags,
																  long_lat);

					send_result = add_record(records, sz_records, &ev, long_lat, IMSI);
					if (send_result == 2) {				// sent OK
						Successful_Xfer_Callback();		// tell flash
						debug_Printf("** POST successful **\n");
					}
					if (send_result == 0) {
						debug_Printf("** POST failed **\n");
						break;							// bail
					}
				}

				if (send_result != 0) {
					if (flush(records)) {					// sent OK
						Successful_Xfer_Callback();			// tell flash
						debug_Printf("** POST successful **\n");
					} else {
						debug_Printf("** POST failed **\n");
					}
				}

				debug_Puts("--- Done! ---");
				LED_ON(GREEN_LED);
				LED_OFF(BLUE_LED);

				free(records);	// oops

				at_sleep();

				// clear hold (only after a delay to let modem quiet down
				vTaskDelay(MS_TO_TICKS(5000));
				gHoldSamplesDuringModemActivity = false;
			}
#endif
		}
	}
}

