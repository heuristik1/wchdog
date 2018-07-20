/*
 * Copyright (c) 2018 Integrated Solutions, Inc.
 * All rights reserved, worldwide.
 *
 */


#define  TEMP_PIN	(1<<10)
#define  VOLT_PIN	(1<<12)
#define  SS_pin		(1<<15)


#define  TEMP_DAT		(GPIOA->IDR & TEMP_PIN)		/*	Data Input pin	*/
#define  VOLT_DAT		(GPIOA->IDR & VOLT_PIN)		/*	Data Input pin	*/
#define  FLASH_DIN		(GPIOA->IDR & (1<<6))		/*	Data Input pin	*/
#define  TAMPER_IN		(GPIOC->IDR & (1<<13))		/*	Data Input pin	*/
#define  MODEM_RI		(GPIOB->IDR &  1    )		/*	Data Input pin	*/
#define  MODEM_NETLIGHT	(GPIOB->IDR & (1<<1))		/*	Data Input pin	*/
#define  MODEM_STATUS	(GPIOB->IDR & (1<<2))		/*	Data Input pin	*/



extern void Hardware_Init();
extern void MODEM_PWRKEY (int x);		//	Set or Clear the Modem PowerKey pin
extern void MODEM_PWR_EN (int x);		//	Set or Clear the Modem Power-Enable pin
extern void FLASH_DOUT (int x);			//	Set or Clear the External Flash Memory SPI Data pin
extern void FLASH_CLK (int x);			//	Set or Clear the External Flash Memory SPI Clock pin
extern void FLASH_CS (int x);			//	Set or Clear the External Flash Memory SPI Chip-Select pin
extern void TEMP_SCL (int x);			//	Set or Clear the A/D's I2C SCL pin
extern void TEMP_SDA (int x);			//	Set or Clear the A/D's I2C SDA pin
extern void VOLT_SCL (int x);			//	Set or Clear the A/D's I2C SCL pin
extern void VOLT_SDA (int x);			//	Set or Clear the A/D's I2C SDA pin
