/*
 * Copyright (c) 2018 Integrated Solutions, Inc.
 * All rights reserved, worldwide.
 *
 */


#include "stm32f0xx_gpio.h"
#include "stm32f0xx_usart.h"

/* Scheduler include files */
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include <string.h>
#include "armtypes.h"
#include "uart5.h"
#include "Debug.h"



#if 0

#define MAX_RBUF_SIZE 256

uint8 rbuf[MAX_RBUF_SIZE] = {0};

void modem_PrintRbuf()
{
	debug_Printf("modem: %s", rbuf);
}

int modem_ResponseHasOK()
{
	if (strstr((const char *)rbuf, "OK\r\n") != NULL)
		return 1;
	return 0;
}

int modem_SendATCommand(const char *pCmd, int timeout_ms)
{
	int cmdlength,nbytes;
	int index=-1;
	int rbyte;
	portTickType xDelayTime = 100 / portTICK_RATE_MS;


	cmdlength = strlen(pCmd);						// check how many characters will echo
	uart5_Puts(pCmd);								// send to modem
	vTaskDelay(xDelayTime);
	nbytes = uart5_Read(rbuf, cmdlength, 0);		// read out echo
	rbuf[nbytes] = 0;

	if (nbytes > 0 && strcmp(pCmd, (const char *)rbuf) == 0)
	{
		// it was echoed, let's wait for response
		index = 0;
		// attempt to grab first char
		rbuf[index] = uart5_WaitForByte(timeout_ms);
		if (rbuf[index] == -1)
			return -1;
		index++;

		while(1)
		{
			rbyte = uart5_WaitForByte(50);
			if (rbyte == -1)
				break;
			rbuf[index] = rbyte;
			index++;
		}
		rbuf[index++] = 0;
		// we should have a response
	}

	return index; 	// return num received or -1
}

#endif




void Modem_Init()
{
	uart5_Init();		//	CELL MODEM UART
}

void Modem_Test()
{
	uart5_Puts("AT\n");
//	uart5_Puts("AT+CGMI\n");
}
