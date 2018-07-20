/*
 * Copyright (c) 2018 Integrated Solutions, Inc.
 * All rights reserved, worldwide.
 *
 */


#include "stm32f0xx_gpio.h"
#include "stm32f0xx_usart.h"
#include "stm32f0xx_rcc.h"

#include <string.h>
#include "Debug.h"


void debug_Init()
{
	uart1_Init();		//	Debug UART
}


void debug_Puts (char const *s)
{
	uart1_Puts(s);
}

void debug_PutLine (char const *s)
{
	uart1_PutLine(s);
}

void debug_Putc (char c)
{
	uart1_PutByte(c);
}

