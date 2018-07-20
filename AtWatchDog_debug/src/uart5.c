/*
 * Copyright (c) 2018 Integrated Solutions, Inc.
 * All rights reserved, worldwide.
 *
 */


#include "stm32f0xx_gpio.h"
#include "stm32f0xx_usart.h"
#include "stm32f0xx_rcc.h"

#include <string.h>
#include <stdarg.h>
#include <stdio.h>

/* Scheduler include files */
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include "micro_vprintf.h"
#include "armtypes.h"
#include "uart5.h"
#include "main.h"	// MS_TO_TICKS()

#define UART5_BAUDRATE 115200

#define RX_BUFSIZE	128
#define TX_BUFSIZE	128

uint8 		uart5_RxBuf[RX_BUFSIZE]; // this will hold the received string
unsigned	uart5_RxBufHead;
unsigned	uart5_RxBufTail;

uint8		uart5_TxBuf[TX_BUFSIZE];
unsigned	uart5_TxBufHead;
unsigned	uart5_TxBufTail;

int			uart5_RxFlags;

#define		RX_FLAG_FRAMING_ERROR		(1 << 0)
#define		RX_FLAG_PARITY_ERROR		(1 << 1)
#define		RX_FLAG_OVERRUN_ERROR		(1 << 2)
#define		RX_FLAG_BUFFER_OVERFLOW		(1 << 3)


xSemaphoreHandle	uart5_TxDrainedSemaphore;
xSemaphoreHandle	uart5_RxAvailableSemaphore;

xSemaphoreHandle	uart5_TxMutex;
xSemaphoreHandle	uart5_RxMutex;


void uart5_Init()
{
	// USART peripheral initialization settings
	USART_InitTypeDef USART_InitStructure;
	GPIO_InitTypeDef GPIO_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure; // this is used to configure the NVIC (nested vector interrupt controller)

	uart5_RxBufHead = 0;
	uart5_RxBufTail = 0;
	uart5_TxBufHead = 0;
	uart5_TxBufTail = 0;
	uart5_RxFlags = 0;

	vSemaphoreCreateBinary(uart5_RxAvailableSemaphore);

	vSemaphoreCreateBinary(uart5_TxDrainedSemaphore);
	xSemaphoreGive(uart5_TxDrainedSemaphore);	// initially drained

	uart5_RxMutex = xSemaphoreCreateMutex();
	uart5_TxMutex = xSemaphoreCreateMutex();


	/* enable APB1 peripheral clock for USART5
	 * note that only USART5 and USART6 are connected to APB2
	 * the other USARTs are connected to APB1
	 */
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART5,ENABLE);

	/* enable the peripheral clock for the pins used by
	 * USART5, PB3 for TX and PB4 for RX
	 */
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOB, ENABLE);

	//Configure USART5 pins: Tx (PB3) and Rx (PB4)
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_4;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	/* The RX and TX pins are now connected to their AF
	 * so that the USART5 can take over control of the
	 * pins
	 */
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource3, GPIO_AF_4);	// Alternate Function 4 == USART5_TX
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource4, GPIO_AF_4);	// Alternate Function 4 == USART5_RX
	//Configure USART5 setting: ----------------------------
	USART_InitStructure.USART_BaudRate = UART5_BAUDRATE;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART5, &USART_InitStructure);

	/* Here the USART5 transmit interrupt is disabled. This is generated when the
	 * transmit data register is empty
	 */
	USART_ITConfig(USART5, USART_IT_TXE, DISABLE); 			// disable the USART5 receive interrupt

	/* Here the USART5 receive interrupt is enabled
	 * and the interrupt controller is configured
	 * to jump to the USART5_IRQHandler() function
	 * if the USART5 receive interrupt occurs
	 */
	USART_ITConfig(USART5, USART_IT_RXNE, ENABLE); // enable the USART5 receive interrupt

	NVIC_InitStructure.NVIC_IRQChannel = USART3_6_IRQn;		 // we want to configure the USART5 interrupts
	NVIC_InitStructure.NVIC_IRQChannelPriority = 0;			 // this sets the priority group of the USART5 interrupts
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;			 // the USART5 interrupts are globally enabled
	NVIC_Init(&NVIC_InitStructure);							 // the properties are passed to the NVIC_Init function which takes care of the low level stuff

	// finally this enables the complete USART5 peripheral
	USART_Cmd(USART5, ENABLE);
}

void uart5_Putc(int c, void *unused)
{
	// wait until data register is empty
	while( USART_GetFlagStatus(USART5, USART_FLAG_TXE) == RESET)
		;
	USART_SendData(USART5,c);
}

int uart5_PutByteNoLock(int ch)
	{
	unsigned newtail;

	newtail = uart5_TxBufTail + 1;
	if (newtail >= TX_BUFSIZE)
		newtail = 0;
	while (newtail == uart5_TxBufHead)
		{
		// no more space in the buffer
		// Make sure the TX interrupt is enabled so the buffer will drain
		USART_ITConfig(USART5, USART_IT_TXE, ENABLE); 			// enable the USART5 receive interrupt
		if (!xSemaphoreTake(uart5_TxDrainedSemaphore,MS_TO_TICKS(100)))
			return 0;
		}
	uart5_TxBuf[uart5_TxBufTail] = ch;
	uart5_TxBufTail = newtail;
	USART_ITConfig(USART5, USART_IT_TXE, ENABLE); 			// enable the USART5 receive interrupt
	return 1;
	}

int uart5_PutByte(int ch)
	{
	int rval;
	xSemaphoreTake(uart5_TxMutex,portMAX_DELAY);
	rval = uart5_PutByteNoLock(ch);
	xSemaphoreGive(uart5_TxMutex);
	return rval;
	}


int uart5_Write(void const *bufp, int nbytes)
	{
	uint8 const *p = (uint8 const *) bufp;
	int nwritten = 0;
	xSemaphoreTake(uart5_TxMutex,portMAX_DELAY);
	while (nwritten < nbytes)
		{
		if (!uart5_PutByteNoLock(p[nwritten]))
			break;
		nwritten++;
		}
	xSemaphoreGive(uart5_TxMutex);
	return nwritten;
	}

void uart5_Puts(char const *s)
	{
	xSemaphoreTake(uart5_TxMutex,portMAX_DELAY);
	while (*s)
		uart5_PutByteNoLock(*s++);
//	uart5_PutByteNoLock('\r');		// for Windows compatibility
//	uart5_PutByteNoLock('\n');
	xSemaphoreGive(uart5_TxMutex);
	}

void uart5_FlushReceive()
{
	if (uart5_RxBufHead != uart5_RxBufTail)
	{
		xSemaphoreTake(uart5_RxAvailableSemaphore,MS_TO_TICKS(10));
		uart5_RxBufHead = uart5_RxBufTail;
	}
}


int uart5_GetByte(void)
	{
	int ch = -1;
	if (uart5_RxBufHead != uart5_RxBufTail)
		{
		unsigned newhead = uart5_RxBufHead + 1;
		if (newhead >= RX_BUFSIZE)
			newhead = 0;
		ch = uart5_RxBuf[uart5_RxBufHead];
		uart5_RxBufHead = newhead;
		}
	return ch;
	}

int uart5_WaitForByte(int timeout_ms)
	{
	int ch = -1;
	// Wait for at least one byte to arrive
	if (uart5_RxBufHead == uart5_RxBufTail && timeout_ms > 0)
		{
		// wait for at least one character
		xSemaphoreTake(uart5_RxAvailableSemaphore,MS_TO_TICKS(timeout_ms));
		}
	if (uart5_RxBufHead != uart5_RxBufTail)
		{
		unsigned newhead = uart5_RxBufHead + 1;
		if (newhead >= RX_BUFSIZE)
			newhead = 0;
		ch = uart5_RxBuf[uart5_RxBufHead];
		uart5_RxBufHead = newhead;
		if (uart5_RxBufHead == uart5_RxBufTail)
			xSemaphoreTake(uart5_RxAvailableSemaphore,MS_TO_TICKS(1));	// take it, we are done
		}
	return ch;
	}

int uart5_Read(void *bufp, int maxbytes, int timeout_ms)
	{
	uint8 *p = (uint8 *) bufp;
	int nread = 0;

	// Wait for at least one byte to arrive
	if (uart5_RxBufHead == uart5_RxBufTail && timeout_ms > 0)
		{
		// wait for at least one character
		xSemaphoreTake(uart5_RxAvailableSemaphore,MS_TO_TICKS(timeout_ms));
		}

	// read as much as we can
	while (nread < maxbytes)
		{
		int ch = uart5_GetByte();
		if (ch == -1)
			break;
		p[nread++] = ch;
		}

	return nread;
	}


int uart5_Printf(char const *fmt, ... )
	{
	va_list arg;
	int n;

	va_start(arg,fmt);
	n = micro_vprintf(uart5_Putc,NULL,fmt,arg);
	va_end(arg);

	return n;
	}



// this is the interrupt request handler (IRQ) for ALL USART5 interrupts
void USART3_6_IRQHandler(void)
{
    signed portBASE_TYPE	should_yield = pdFALSE;
    FlagStatus status;

    // Is this an RX interrupt?
    if( USART_GetITStatus(USART5, USART_IT_RXNE) )
    {
        while ( (status=USART_GetFlagStatus(USART5, USART_FLAG_RXNE)) )
        {
            uart5_RxBuf[uart5_RxBufTail] = USART5->RDR; // read character, resets flag
            if (++uart5_RxBufTail >= RX_BUFSIZE)
                uart5_RxBufTail = 0;
            if (uart5_RxBufTail == uart5_RxBufHead)
                uart5_RxFlags |= RX_FLAG_BUFFER_OVERFLOW;
            //			if (status & UART_PARITY_ERROR)
            //				uart5_RxFlags |= RX_FLAG_PARITY_ERROR;
            //			if (status & UART_FRAMING_ERROR)
            //				uart5_RxFlags |= RX_FLAG_FRAMING_ERROR;
        }
        status=USART_GetFlagStatus(USART5, USART_FLAG_ORE);
        if (status)
        {
            USART_ClearFlag(USART5, USART_FLAG_ORE);
            uart5_RxFlags |= RX_FLAG_OVERRUN_ERROR;
        }
        xSemaphoreGiveFromISR(uart5_RxAvailableSemaphore,&should_yield);	// new data is available
    }

    // Is this a TX interrupt?
    if( USART_GetITStatus(USART5, USART_IT_TXE) )
    {
        while ( (status=USART_GetFlagStatus(USART5, USART_FLAG_TXE)) )
        {
            if (uart5_TxBufHead == uart5_TxBufTail)
            {
                // no more data pending, disable the TX interrupt
                USART_ITConfig(USART5, USART_IT_TXE, DISABLE); 			// disable the USART5 transmit interrupt
                break;
            }
            else
            {
                USART_SendData(USART5,uart5_TxBuf[uart5_TxBufHead]);
                if (++uart5_TxBufHead >= TX_BUFSIZE)
                    uart5_TxBufHead = 0;
            }
        }
        xSemaphoreGiveFromISR(uart5_TxDrainedSemaphore,&should_yield);	// now drained, at least partially
    }

    if (should_yield != pdFALSE)
    {
        taskYIELD();
    }

}

