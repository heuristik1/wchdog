/*
 * Copyright (c) 2018 Integrated Solutions, Inc.
 * All rights reserved, worldwide.
 *
 */


#include  "stm32f0xx.h"
#include  "stm32f0xx_gpio.h"


/**
  * @brief	Turn specified LED on
  * @param	None
  * @retval None
  */
void LED_ON (int LED_pin)
{
	GPIO_WriteBit (GPIOB, LED_pin, Bit_RESET);
}


/**
  * @brief	Turn specified LED off
  * @param	None
  * @retval None
  */
void LED_OFF (int LED_pin)
{
	GPIO_WriteBit (GPIOB, LED_pin, Bit_SET);
}

