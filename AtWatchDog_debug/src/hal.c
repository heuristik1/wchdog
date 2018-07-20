/*
 * Copyright (c) 2018 Integrated Solutions, Inc.
 * All rights reserved, worldwide.
 *
 */


#include "stm32f0xx_rcc.h"
#include "stm32f0xx_gpio.h"
#include "main.h"
#include "flash.h"
#include "LED.h"
#include "hal.h"


typedef struct
{
	GPIO_TypeDef *		Port;			//	GPIOA .. GPIOF
	uint32				Pin;			//	GPIO_Pin_0 .. GPIO_Pin_15
	GPIOMode_TypeDef	Mode;			//	GPIO_Mode_IN  GPIO_Mode_OUT  GPIO_Mode_AF  GPIO_Mode_AN
	GPIOSpeed_TypeDef	Speed;			//	GPIO_Speed_Level_1	GPIO_Speed_Level_2	GPIO_Speed_Level_3
	GPIOOType_TypeDef	PP_OD;			//	GPIO_OType_PP	GPIO_OType_OD
	GPIOPuPd_TypeDef	PU_PD;			//	GPIO_PuPd_NOPULL  GPIO_PuPd_UP	GPIO_PuPd_DOWN
} GPIO_Pin_Defn;


#define  GPIO_COUNT		33

const GPIO_Pin_Defn  Pin_Defns[GPIO_COUNT] =
{
	{	GPIOA,	0, GPIO_Mode_OUT, GPIO_Speed_Level_1, GPIO_OType_PP, GPIO_PuPd_NOPULL	},	//	unused
	{	GPIOA,	1, GPIO_Mode_OUT, GPIO_Speed_Level_1, GPIO_OType_PP, GPIO_PuPd_NOPULL	},	//	MODEM PWRKEY
	{	GPIOA,	2, GPIO_Mode_AN,  GPIO_Speed_Level_1, GPIO_OType_PP, GPIO_PuPd_NOPULL	},	//	Volt In  (analog)
	{	GPIOA,	3, GPIO_Mode_AN,  GPIO_Speed_Level_1, GPIO_OType_PP, GPIO_PuPd_NOPULL	},	//	Temp In  (analog)
	{	GPIOA,	4, GPIO_Mode_OUT, GPIO_Speed_Level_1, GPIO_OType_PP, GPIO_PuPd_NOPULL	},	//	unused
	{	GPIOA,	5, GPIO_Mode_AF,  GPIO_Speed_Level_2, GPIO_OType_PP, GPIO_PuPd_NOPULL	},	//	Ext.Flash  SPI-1 CLK
	{	GPIOA,	6, GPIO_Mode_AF,  GPIO_Speed_Level_2, GPIO_OType_PP, GPIO_PuPd_NOPULL	},	//	Ext.Flash  SPI-1 MISO
	{	GPIOA,	7, GPIO_Mode_AF,  GPIO_Speed_Level_2, GPIO_OType_PP, GPIO_PuPd_NOPULL	},	//	Ext.Flash  SPI-1 MOSI
	{	GPIOA,	8, GPIO_Mode_OUT, GPIO_Speed_Level_1, GPIO_OType_PP, GPIO_PuPd_NOPULL	},	//	unused
	{	GPIOA,	9, GPIO_Mode_OUT, GPIO_Speed_Level_2, GPIO_OType_OD, GPIO_PuPd_NOPULL	},	//	Temp ADC  I2C-1 CLK
	{	GPIOA, 10, GPIO_Mode_OUT, GPIO_Speed_Level_2, GPIO_OType_OD, GPIO_PuPd_NOPULL	},	//	Temp ADC  I2C-1 DAT
	{	GPIOA, 11, GPIO_Mode_OUT, GPIO_Speed_Level_2, GPIO_OType_OD, GPIO_PuPd_NOPULL	},	//	Volt ADC  I2C-2 CLK
	{	GPIOA, 12, GPIO_Mode_OUT, GPIO_Speed_Level_2, GPIO_OType_OD, GPIO_PuPd_NOPULL	},	//	Volt ADC  I2C-2 DAT
//	{	GPIOA, 13, GPIO_Mode_IN,  GPIO_Speed_Level_1, GPIO_OType_PP, GPIO_PuPd_UP		},	//	Pgm Pins: Data
//	{	GPIOA, 14, GPIO_Mode_IN,  GPIO_Speed_Level_1, GPIO_OType_PP, GPIO_PuPd_DOWN		},	//	Pgm Pins: Clock
	{	GPIOA, 15, GPIO_Mode_OUT, GPIO_Speed_Level_2, GPIO_OType_PP, GPIO_PuPd_NOPULL	},	//	Ext.Flash  SPI-1 -CS
	{	GPIOB,	0, GPIO_Mode_IN,  GPIO_Speed_Level_1, GPIO_OType_PP, GPIO_PuPd_NOPULL	},	//	MODEM UART RI
	{	GPIOB,	1, GPIO_Mode_IN,  GPIO_Speed_Level_1, GPIO_OType_PP, GPIO_PuPd_NOPULL	},	//	MODEM NETLIGHT
	{	GPIOB,	2, GPIO_Mode_IN,  GPIO_Speed_Level_1, GPIO_OType_PP, GPIO_PuPd_NOPULL	},	//	MODEM STATUS
	{	GPIOB,	3, GPIO_Mode_AF,  GPIO_Speed_Level_2, GPIO_OType_PP, GPIO_PuPd_NOPULL	},	//	MODEM UART-5 Tx
	{	GPIOB,	4, GPIO_Mode_AF,  GPIO_Speed_Level_2, GPIO_OType_PP, GPIO_PuPd_NOPULL	},	//	MODEM UART-5 Rx
	{	GPIOB,	5, GPIO_Mode_OUT, GPIO_Speed_Level_1, GPIO_OType_PP, GPIO_PuPd_NOPULL	},	//	unused
	{	GPIOB,	6, GPIO_Mode_AF,  GPIO_Speed_Level_2, GPIO_OType_PP, GPIO_PuPd_NOPULL	},	//	Debug UART-1 Tx
	{	GPIOB,	7, GPIO_Mode_AF,  GPIO_Speed_Level_2, GPIO_OType_PP, GPIO_PuPd_NOPULL	},	//	Debug UART-1 Rx
	{	GPIOB,	8, GPIO_Mode_OUT, GPIO_Speed_Level_2, GPIO_OType_PP, GPIO_PuPd_NOPULL	},	//	MODEM PWR
	{	GPIOB,	9, GPIO_Mode_OUT, GPIO_Speed_Level_1, GPIO_OType_PP, GPIO_PuPd_NOPULL	},	//	unused
	{	GPIOB, 10, GPIO_Mode_OUT, GPIO_Speed_Level_1, GPIO_OType_PP, GPIO_PuPd_NOPULL	},	//	unused (to daughter board)
	{	GPIOB, 11, GPIO_Mode_OUT, GPIO_Speed_Level_1, GPIO_OType_PP, GPIO_PuPd_NOPULL	},	//	unused (to daughter board)
	{	GPIOB, 12, GPIO_Mode_OUT, GPIO_Speed_Level_3, GPIO_OType_PP, GPIO_PuPd_NOPULL	},	//	LED -BLUE
	{	GPIOB, 13, GPIO_Mode_OUT, GPIO_Speed_Level_1, GPIO_OType_PP, GPIO_PuPd_NOPULL	},	//	unused (to daughter board)
	{	GPIOB, 14, GPIO_Mode_OUT, GPIO_Speed_Level_3, GPIO_OType_PP, GPIO_PuPd_NOPULL	},	//	LED -RED
	{	GPIOB, 15, GPIO_Mode_OUT, GPIO_Speed_Level_3, GPIO_OType_PP, GPIO_PuPd_NOPULL	},	//	LED -GREEN
	{	GPIOC, 13, GPIO_Mode_IN,  GPIO_Speed_Level_1, GPIO_OType_PP, GPIO_PuPd_NOPULL	},	//	Tamper Input
	{	GPIOC, 14, GPIO_Mode_OUT, GPIO_Speed_Level_1, GPIO_OType_PP, GPIO_PuPd_NOPULL	},	//	32 KHz		LSE_ON overrides these settings
	{	GPIOC, 15, GPIO_Mode_OUT, GPIO_Speed_Level_1, GPIO_OType_PP, GPIO_PuPd_NOPULL	}	//	32 KHz
};

// ------------------------------------------------------------------------------------------

void Clocks_Init()								//	Enable clocks to the various sub-systems
{
	RCC_AHBPeriphClockCmd (RCC_AHBPeriph_GPIOA, ENABLE);	//	Enable GPIO clock
	RCC_AHBPeriphClockCmd (RCC_AHBPeriph_GPIOB, ENABLE);	//	Enable GPIO clock
	RCC_AHBPeriphClockCmd (RCC_AHBPeriph_GPIOC, ENABLE);	//	Enable GPIO clock
	RCC_AHBPeriphClockCmd (RCC_AHBPeriph_DMA1,	ENABLE);
	RCC_AHBPeriphClockCmd (RCC_AHBPeriph_CRC,	ENABLE);

//	RCC_APB1PeriphClockCmd (RCC_APB1Periph_TIM3,   ENABLE);		///	TODO: delete what's not needed...
//	RCC_APB1PeriphClockCmd (RCC_APB1Periph_TIM6,   ENABLE);
//	RCC_APB1PeriphClockCmd (RCC_APB1Periph_TIM14,  ENABLE);
//	RCC_APB1PeriphClockCmd (RCC_APB1Periph_WWDG,   ENABLE);
	RCC_APB1PeriphClockCmd (RCC_APB1Periph_USART2, ENABLE);
//	RCC_APB1PeriphClockCmd (RCC_APB1Periph_I2C1,   ENABLE);
//	RCC_APB1PeriphClockCmd (RCC_APB1Periph_I2C2,   ENABLE);
	RCC_APB1PeriphClockCmd (RCC_APB1Periph_PWR,	   ENABLE);

	RCC_APB2PeriphClockCmd (RCC_APB2Periph_SYSCFG, ENABLE);		///	TODO: delete what's not needed...
	RCC_APB2PeriphClockCmd (RCC_APB2Periph_TIM1,   ENABLE);
	RCC_APB2PeriphClockCmd (RCC_APB2Periph_SPI1,   ENABLE);
	RCC_APB2PeriphClockCmd (RCC_APB2Periph_USART1, ENABLE);
//	RCC_APB2PeriphClockCmd (RCC_APB2Periph_TIM15,  ENABLE);
//	RCC_APB2PeriphClockCmd (RCC_APB2Periph_TIM16,  ENABLE);
//	RCC_APB2PeriphClockCmd (RCC_APB2Periph_TIM17,  ENABLE);
	RCC_APB2PeriphClockCmd (RCC_APB2Periph_DBGMCU, ENABLE);
}


/**
  * @brief	Initialize the I/O ports and internal peripherals.
  * @param	None
  * @retval None
  */
void Hardware_Init()
{
	int  i, pin;
	GPIO_InitTypeDef GPIO_InitStructure;

	Clocks_Init();					//	Enable clocks to the various sub-systems
									//	Init output pins as low-level
	GPIO_Write (GPIOA, SS_pin);		//	Port A = all low except -SS pin
	GPIO_Write (GPIOB, 0);			//	Port B = all low   (ALL LEDs ON)
	GPIO_Write (GPIOC, 0);			//	Port C = all low

	for (i=0; i<GPIO_COUNT; i++)
	{
		pin = 1 << Pin_Defns[i].Pin;
		GPIO_InitStructure.GPIO_Pin	  = pin;
		GPIO_InitStructure.GPIO_Mode  = Pin_Defns[i].Mode;
		GPIO_InitStructure.GPIO_Speed = Pin_Defns[i].Speed;
		GPIO_InitStructure.GPIO_OType = Pin_Defns[i].PP_OD;
		GPIO_InitStructure.GPIO_PuPd  = Pin_Defns[i].PU_PD;
		GPIO_Init (Pin_Defns[i].Port, &GPIO_InitStructure);
	}

	Flash_Disable();				//	This pin normally HI
	uS_Delay (500000);				//	1/2 second delay with all LEDs on
	LED_OFF (GREEN_LED);			//	Initial state: disconnected from cell
	LED_OFF (RED_LED);				//		(Red LED off)
	LED_OFF (BLUE_LED);				//		(Blue LED off)
}


void MODEM_PWRKEY (int x)			//	Set or Clear the Modem PowerKey pin
{
	if (x)	GPIOA->BSRR = (1<<1);
	else	GPIOA->BRR = (1<<1);
}


void FLASH_CLK (int x)				//	Set or Clear the External Flash Memory SPI Clock pin
{
	if (x)	GPIOA->BSRR = (1<<5);
	else	GPIOA->BRR = (1<<5);
}


void FLASH_DOUT (int x)				//	Set or Clear the External Flash Memory SPI Data pin
{
	if (x)	GPIOA->BSRR = (1<<7);
	else	GPIOA->BRR = (1<<7);
}


void FLASH_CS (int x)				//	Set or Clear the External Flash Memory SPI Chip-Select pin
{
	if (x)	GPIOA->BSRR = (1<<15);
	else	GPIOA->BRR = (1<<15);
}


void TEMP_SCL (int x)				//	Set or Clear the A/D's I2C SCL pin
{
	if (x)	GPIOA->BSRR = (1<<9);
	else	GPIOA->BRR = (1<<9);
}


void TEMP_SDA (int x)				//	Set or Clear the A/D's I2C SDA pin
{
	if (x)	GPIOA->BSRR = (1<<10);
	else	GPIOA->BRR = (1<<10);
}


void VOLT_SCL (int x)				//	Set or Clear the A/D's I2C SCL pin
{
	if (x)	GPIOA->BSRR = (1<<11);
	else	GPIOA->BRR = (1<<11);
}


void VOLT_SDA (int x)				//	Set or Clear the A/D's I2C SDA pin
{
	if (x)	GPIOA->BSRR = (1<<12);
	else	GPIOA->BRR = (1<<12);
}


void MODEM_PWR_EN (int x)			//	Set or Clear the Modem Power-Enable pin
{
	if (x)	GPIOB->BSRR = (1<<8);
	else	GPIOB->BRR = (1<<8);
}

