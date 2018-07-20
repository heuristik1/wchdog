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
extern int g_modemState;

#include "gsmconfig.h"

// hold sampling until modem connects
bool gHoldSamplesDuringModemActivity = true;

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

int get_time_and_location(uint32 *pt, char *long_lat, int cb_long_lat)
{
    int i;

    *pt = 0;

    // get time and location
    for (i = 0; i < 10 && *pt == 0; i++) {
        vTaskDelay(MS_TO_TICKS(2000));
        gsm_get_location_and_time (long_lat, cb_long_lat, pt);
    }

    if (*pt == 0) {
        debug_Printf("failed to get time/location from modem");
        return -1;
    } else {
        debug_Printf("time from modem %d, location %s", *pt, long_lat);
    }

    return 0;
}

char records[1024] = {0};
void send_records(char *IMSI)
{
	Event_t ev;
	uint32 t=0;
	int result=0;
	static char long_lat[128];

    if (get_time_and_location(&t, long_lat, sizeof(long_lat)) != 0)
    {
        return;
    }

    Time_Callback(t);	// sync time    // TODO: This name might suck. what does it do?

    debug_Puts("--- Sending ---");
   
    Begin_Report();	 // have flash prepare the reports

    while (Report_Callback(&ev) != -1) 
    {
        result = add_record(records, sizeof(records), &ev, long_lat, IMSI);

        if (result == 2) {
            Successful_Xfer_Callback();		// send ok, tell flash
            debug_Printf("**** POST successful, Wrote Page ****\n");
        }

        if (result == 0) {
            debug_Printf("**** POST failed ****\n");
            break;							// bail
        }
    }

    // flush/post final page
    if (result != 0) {
        if (flush(records)) {
            Successful_Xfer_Callback();			// sent ok, tell flash
            debug_Printf("**** POST successful, Wrote Page ****\n");
        } else {
            debug_Printf("** POST failed **\n");
        }
    }

    debug_Puts("--- Done Send_Records ---");
}

// Wait for task in main() to finish initial setup, and set state to 1.
void WaitForModemInit()
{
    while (1)
    {
        vTaskDelay(MS_TO_TICKS(1000));
        if (g_modemState == 1)
        {
            break;
        }
    }
}

void prvCellTask( void *pvParameters )
{
    static char IMSI[32];
    static char long_lat[128];
    uint32 t=0;

    WaitForModemInit();

    debug_Printf("--- Do final modem prep ---");
    gsm_set_apn(g_apn);

    // AT init and get time
    do {
        do_init();
        at_wake();

        LED_OFF(GREEN_LED);
        LED_ON(RED_LED);
        vTaskDelay(MS_TO_TICKS(500));

        gsm_get_location_and_time (long_lat, sizeof(long_lat), &t);

        LED_ON(GREEN_LED);
        LED_OFF(RED_LED);
        vTaskDelay(MS_TO_TICKS(500));
    } while(t==0);

    LED_OFF(RED_LED);		// red off if talking to modem
    LED_ON(GREEN_LED);		// solid on after connect

    Time_Callback(t);		// set the time for the flash logger
    gsm_get_IMSI(IMSI, sizeof(IMSI));

    // let main thread know we are ready to rock
    debug_Printf("--- Modem prep complete ---");
    g_modemState = 2;

    debug_Puts("--- Sleep ---");
    at_sleep();

    // Wait a bit so let modem simmer down before measuring again
    vTaskDelay(MS_TO_TICKS(2000));
    gHoldSamplesDuringModemActivity = false;

    for( ;; )
    {
        // wait for the semaphore from the main task
        if (xSemaphoreTake(connect_Semaphore, MS_TO_TICKS(Config.Report_Rate * 1000)) == pdTRUE )
        {
            // Stop taking measurements while using the modem
            gHoldSamplesDuringModemActivity = true;
            vTaskDelay(MS_TO_TICKS(3000));

            LED_OFF(GREEN_LED);
            LED_ON(BLUE_LED);

            // Wake up the modem
            debug_Puts("--- Wake Up ---");
            if (at_wake())
            {
                debug_Puts("--- Wake Up Successful, Sending Records ---");
                send_records(IMSI);
            }
            else
            {
                debug_Puts("--- Wake Up Failed, Not Sending Records ---");
            }

            LED_ON(GREEN_LED);
            LED_OFF(BLUE_LED);

            debug_Puts("--- Sleep ---");
            at_sleep();

            // Wait a bit so let modem simmer down before measuring again
            vTaskDelay(MS_TO_TICKS(2000));
            gHoldSamplesDuringModemActivity = false;
        }
    }
}

