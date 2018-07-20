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
#include "uart1.h"
#include "main.h"	// MS_TO_TICKS()

#define UART1_BAUDRATE 115200

#define RX_BUFSIZE	128
#define TX_BUFSIZE	128

uint8 		uart1_RxBuf[RX_BUFSIZE]; // this will hold the received string
unsigned	uart1_RxBufHead;
unsigned	uart1_RxBufTail;

uint8		uart1_TxBuf[TX_BUFSIZE];
unsigned	uart1_TxBufHead;
unsigned	uart1_TxBufTail;

int			uart1_RxFlags;

#define		RX_FLAG_FRAMING_ERROR		(1 << 0)
#define		RX_FLAG_PARITY_ERROR		(1 << 1)
#define		RX_FLAG_OVERRUN_ERROR		(1 << 2)
#define		RX_FLAG_BUFFER_OVERFLOW		(1 << 3)


xSemaphoreHandle	uart1_TxDrainedSemaphore;
xSemaphoreHandle	uart1_RxAvailableSemaphore;

xSemaphoreHandle	uart1_TxMutex;
xSemaphoreHandle	uart1_RxMutex;


void uart1_Init()
{
	// USART peripheral initialization settings
	USART_InitTypeDef USART_InitStructure;
	GPIO_InitTypeDef GPIO_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure; // this is used to configure the NVIC (nested vector interrupt controller)

	uart1_RxBufHead = 0;
	uart1_RxBufTail = 0;
	uart1_TxBufHead = 0;
	uart1_TxBufTail = 0;
	uart1_RxFlags = 0;

	vSemaphoreCreateBinary(uart1_RxAvailableSemaphore);

	vSemaphoreCreateBinary(uart1_TxDrainedSemaphore);
	xSemaphoreGive(uart1_TxDrainedSemaphore);	// initially drained

	uart1_RxMutex = xSemaphoreCreateMutex();
	uart1_TxMutex = xSemaphoreCreateMutex();

	/* enable APB2 peripheral clock for USART1
	 * note that only USART1 and USART6 are connected to APB2
	 * the other USARTs are connected to APB1
	 */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1,ENABLE);

	/* enable the peripheral clock for the pins used by
	 * USART1, PB6 for TX and PB7 for RX
	 */
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOB, ENABLE);

	//Configure USART1 pins: Tx (PB6) and Rx (PB7)
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	/* The RX and TX pins are now connected to their AF
	 * so that the USART1 can take over control of the
	 * pins
	 */
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource6, GPIO_AF_0);	// Alternate Function 0 == USART1_TX
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource7, GPIO_AF_0);	// Alternate Function 0 == USART1_RX
	//Configure USART1 setting: ----------------------------
	USART_InitStructure.USART_BaudRate = UART1_BAUDRATE;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART1, &USART_InitStructure);

	/* Here the USART1 transmit interrupt is disabled. This is generated when the
	 * transmit data register is empty
	 */
	USART_ITConfig(USART1, USART_IT_TXE, DISABLE); 			// disable the USART1 receive interrupt

	/* Here the USART1 receive interrupt is enabled
	 * and the interrupt controller is configured
	 * to jump to the USART1_IRQHandler() function
	 * if the USART1 receive interrupt occurs
	 */
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE); 			// enable the USART1 receive interrupt

	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;		 // we want to configure the USART1 interrupts
	NVIC_InitStructure.NVIC_IRQChannelPriority = 0;			 // this sets the priority group of the USART1 interrupts
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;			 // the USART1 interrupts are globally enabled
	NVIC_Init(&NVIC_InitStructure);							 // the properties are passed to the NVIC_Init function which takes care of the low level stuff

	// finally this enables the complete USART1 peripheral
	USART_Cmd(USART1, ENABLE);
}

void uart1_Putc(int c, void *unused)
{
	// wait until data register is empty
	while( USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET)
		;
	USART_SendData(USART1,c);
}

int uart1_PutByteNoLock(int ch)
	{
	unsigned newtail;

	newtail = uart1_TxBufTail + 1;
	if (newtail >= TX_BUFSIZE)
		newtail = 0;
	while (newtail == uart1_TxBufHead)
		{
		// no more space in the buffer
		// Make sure the TX interrupt is enabled so the buffer will drain
		USART_ITConfig(USART1, USART_IT_TXE, ENABLE); 			// enable the USART1 receive interrupt
		if (!xSemaphoreTake(uart1_TxDrainedSemaphore,MS_TO_TICKS(100)))
			return 0;
		}
	uart1_TxBuf[uart1_TxBufTail] = ch;
	uart1_TxBufTail = newtail;
	USART_ITConfig(USART1, USART_IT_TXE, ENABLE); 			// enable the USART1 receive interrupt
	return 1;
	}

int uart1_PutByte(int ch)
	{
	int rval;
	xSemaphoreTake(uart1_TxMutex,portMAX_DELAY);
	rval = uart1_PutByteNoLock(ch);
	xSemaphoreGive(uart1_TxMutex);
	return rval;
	}


int uart1_Write(void const *bufp, int nbytes)
	{
	uint8 const *p = (uint8 const *) bufp;
	int nwritten = 0;
	xSemaphoreTake(uart1_TxMutex,portMAX_DELAY);
	while (nwritten < nbytes)
		{
		if (!uart1_PutByteNoLock(p[nwritten]))
			break;
		nwritten++;
		}
	xSemaphoreGive(uart1_TxMutex);
	return nwritten;
	}

void uart1_Puts(char const *s)
	{
	xSemaphoreTake(uart1_TxMutex,portMAX_DELAY);
	while (*s)
		uart1_PutByteNoLock(*s++);
	//uart1_PutByteNoLock('\r');		// for Windows compatibility
	//uart1_PutByteNoLock('\n');
	xSemaphoreGive(uart1_TxMutex);
	}

void uart1_PutLine(char const *s)
	{
	xSemaphoreTake(uart1_TxMutex,portMAX_DELAY);
	while (*s)
		uart1_PutByteNoLock(*s++);
	uart1_PutByteNoLock('\r');		// for Windows compatibility
	uart1_PutByteNoLock('\n');
	xSemaphoreGive(uart1_TxMutex);
	}


int uart1_GetByte(void)
	{
	int ch = -1;
	if (uart1_RxBufHead != uart1_RxBufTail)
		{
		unsigned newhead = uart1_RxBufHead + 1;
		if (newhead >= RX_BUFSIZE)
			newhead = 0;
		ch = uart1_RxBuf[uart1_RxBufHead];
		uart1_RxBufHead = newhead;
		}
	return ch;
	}

int uart1_WaitForByte(int timeout_ms)
	{
	int ch = -1;
	// Wait for at least one byte to arrive
	if (uart1_RxBufHead == uart1_RxBufTail && timeout_ms > 0)
		{
		// wait for at least one character
		xSemaphoreTake(uart1_RxAvailableSemaphore,MS_TO_TICKS(timeout_ms));
		}
	if (uart1_RxBufHead != uart1_RxBufTail)
		{
		unsigned newhead = uart1_RxBufHead + 1;
		if (newhead >= RX_BUFSIZE)
			newhead = 0;
		ch = uart1_RxBuf[uart1_RxBufHead];
		uart1_RxBufHead = newhead;
		if (uart1_RxBufHead == uart1_RxBufTail)
			xSemaphoreTake(uart1_RxAvailableSemaphore,MS_TO_TICKS(1));	// take it, we are done
		}
	return ch;
	}

int uart1_Read(void *bufp, int maxbytes, int timeout_ms)
	{
	uint8 *p = (uint8 *) bufp;
	int nread = 0;

	// Wait for at least one byte to arrive
	if (uart1_RxBufHead == uart1_RxBufTail && timeout_ms > 0)
		{
		// wait for at least one character
		xSemaphoreTake(uart1_RxAvailableSemaphore,MS_TO_TICKS(timeout_ms));
		}

	// read as much as we can
	while (nread < maxbytes)
		{
		int ch = uart1_GetByte();
		if (ch == -1)
			break;
		p[nread++] = ch;
		}

	return nread;
	}


int uart1_Printf(char const *fmt, ... )
	{
	va_list arg;
	int n;

	va_start(arg,fmt);
	n = micro_vprintf(uart1_Putc,NULL,fmt,arg);
	va_end(arg);

	return n;
	}



// this is the interrupt request handler (IRQ) for ALL USART1 interrupts
void USART1_IRQHandler(void)
{
	signed portBASE_TYPE	should_yield = pdFALSE;
	FlagStatus status;

	// Is this an RX interrupt?
	if( USART_GetITStatus(USART1, USART_IT_RXNE) )
		{
		while ( (status=USART_GetFlagStatus(USART1, USART_FLAG_RXNE)) )
			{
			uart1_RxBuf[uart1_RxBufTail] = USART1->RDR; // read character, resets flag
			if (++uart1_RxBufTail >= RX_BUFSIZE)
				uart1_RxBufTail = 0;
			if (uart1_RxBufTail == uart1_RxBufHead)
				uart1_RxFlags |= RX_FLAG_BUFFER_OVERFLOW;
//			if (status & UART_PARITY_ERROR)
//				uart1_RxFlags |= RX_FLAG_PARITY_ERROR;
//			if (status & UART_FRAMING_ERROR)
//				uart1_RxFlags |= RX_FLAG_FRAMING_ERROR;
			}
		status=USART_GetFlagStatus(USART1, USART_FLAG_ORE);
		if (status)
			{
			USART_ClearFlag(USART1, USART_FLAG_ORE);
			uart1_RxFlags |= RX_FLAG_OVERRUN_ERROR;
			}
		xSemaphoreGiveFromISR(uart1_RxAvailableSemaphore,&should_yield);	// new data is available
		}

	// Is this a TX interrupt?
	if( USART_GetITStatus(USART1, USART_IT_TXE) )
		{
		while ( (status=USART_GetFlagStatus(USART1, USART_FLAG_TXE)) )
			{
			if (uart1_TxBufHead == uart1_TxBufTail)
				{
				// no more data pending, disable the TX interrupt
				USART_ITConfig(USART1, USART_IT_TXE, DISABLE); 			// disable the USART1 transmit interrupt
				break;
				}
			else
				{
				USART_SendData(USART1,uart1_TxBuf[uart1_TxBufHead]);
				if (++uart1_TxBufHead >= TX_BUFSIZE)
					uart1_TxBufHead = 0;
				}
			}
		xSemaphoreGiveFromISR(uart1_TxDrainedSemaphore,&should_yield);	// now drained, at least partially
		}

	if (should_yield != pdFALSE)
		{
		taskYIELD();
		}

}
