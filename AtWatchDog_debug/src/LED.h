/*
 * Copyright (c) 2018 Integrated Solutions, Inc.
 * All rights reserved, worldwide.
 *
 */


// #include  "stm32f0xx.h"


#define	GREEN_LED	(1 << 15)
#define	BLUE_LED    (1 << 12)
#define	RED_LED     (1 << 14)


extern void LED_ON (int LED_pin);
extern void LED_OFF (int LED_pin);
