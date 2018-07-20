/*
 * Copyright (c) 2018 Integrated Solutions, Inc.
 * All rights reserved, worldwide.
 *
 */


#include "main.h"
#include "stm32f0xx_gpio.h"
#include "hal.h"
#include "process.h"
#include "Debug.h"


static uint32	sumV;
static uint32	nV;
static uint32	AvgV;
static uint32	AbsMaxV;		//	Absolute maximum valid Voltage, given the battery size
static uint32	AbsMinV;		//	Absolute minimum valid Voltage, given the battery size


static void START_I2C()
{
	VOLT_SDA(1);				//	Send repeated-start if necessary
	VOLT_SCL(1);
	uS_Delay(3);
	VOLT_SDA(0);				//	Do START bit
	uS_Delay(3);
	VOLT_SCL(0);
	uS_Delay(5);
}


static void STOP_I2C()
{
	VOLT_SDA(0);
	uS_Delay(3);
	VOLT_SCL(1);
	uS_Delay(3);
	VOLT_SDA(1);
	uS_Delay(5);
}


static void ACK_I2C()			//	ACK ==> continue reading
{
	VOLT_SDA(0);
	VOLT_SCL(1);
	uS_Delay(3);
	VOLT_SCL(0);
	VOLT_SDA(1);				//	allow slave's data to come in again
}


static void NACK_I2C()			//	NACK ==> discontinue read
{
	VOLT_SDA(1);
	uS_Delay(3);
	VOLT_SCL(1);
	uS_Delay(3);
	VOLT_SCL(0);
}


static bool WRITE_I2C_BYTE (int c)
{
	int  n;
	bool b;

	for (n=0; n<8; n++)
	{
		if (c & 0x80)			//	Shift out the data, MSB first...
			VOLT_SDA(1);
		else
			VOLT_SDA(0);

		c <<= 1;
		VOLT_SCL(1);
		uS_Delay(3);
		VOLT_SCL(0);
	}
	VOLT_SDA(1);				//	Set data line HI so ACK can come in
	uS_Delay(3);
	VOLT_SCL(1);				//	Clock in the device's ACK bit
	uS_Delay(5);
	b = true;
	if (VOLT_DAT)
		b = false;
	VOLT_SCL(0);
	return	b;
}


static int READ_I2C_BYTE()
{
	int	n=8;
	int c=0;

	while (--n >= 0)
	{
		c <<= 1;
		if (VOLT_DAT)			//	Shift in the data, MSB first...
			c++;
		VOLT_SCL(1);
		uS_Delay(3);
		VOLT_SCL(0);
	}
	return	c;
}


bool Read_Volts (uint32 *v)
{
	bool b;
	int  a;
	int  msb, lsb;

	a = 0x29;						//	Perform a read access
	if (Set_Volt_Update_Rate)
		a--;						//	If time to set the update rate, make it a write access

	START_I2C();
	b = WRITE_I2C_BYTE (a);
	if (!b)
	{
		STOP_I2C();					//	A/D still converting: close the bus transaction and exit
		return	false;
	}

	if (Set_Volt_Update_Rate)
	{
		Set_Volt_Update_Rate = false;
		b = WRITE_I2C_BYTE (1);		//	Set conversion rate to 30 Hz
		START_I2C();				//	This is actually a repeated-start condition
		b &= WRITE_I2C_BYTE(0x29);	//	Address the A/D again, for a read access

		if (!b)
		{
			STOP_I2C();				//	A/D stopped talking: close the bus transaction and exit
			return	false;
		}
	}

	msb = READ_I2C_BYTE();			//	MSB of result
	ACK_I2C();
	lsb = READ_I2C_BYTE();			//	LSB of result
	NACK_I2C();
	STOP_I2C();

	*v = msb & 0xFF;
	*v <<= 8;
	*v += lsb & 0xFF;

	if (*v == 0 || *v == 0xFFFF)
	{
		*v = 0;
		return	false;				//	Invalid reading
	}
	return	true;					//	Valid reading
}


static int32 ADC_To_Volts (int32 adc)
{
	return (adc * 8442) >> 13;		//	Convert ADC value to voltage    LSB = 1mV
}


static int32 Volts_To_ADC (int v)	//	Volts in, corresponding A/D value out
{
	int32 t = v;
	return (t << 13) / 8442;		//	Convert ADC value to voltage    LSB = 1mV
}


int  Get_Volts()					//	OUT:	battery voltage, averaged over previous second.  LSB=1mV
{
	int32  v;

	if (nV > 0)
		v = sumV / nV;
	else
		v = AvgV;					//	No average available: use last second's value

	AvgV = v;
	sumV = 0;						//	Ready for next average
	nV = 0;

	v = ADC_To_Volts (v);			//	Convert to voltage    LSB = 1mV
	return	v;
}


void Handle_Volts()
{
	uint32  v;

	if (Read_Volts (&v))			//	Ignore process if data not available yet
	{								//	Validate the A/D value
		if ((v < AbsMaxV) && (v > AbsMinV))
		{
			sumV += v;				//	Add to the average
			nV++;
		}
#if 0	// TODO: testing for anomaly, remove later
		if (gPOR == 0)
		{
//			if (v < 12230 || v > 12250) // 25% lower?
				// did we read 10% off?
				debug_Printf("*** v: %d ***\r\n", v);
		}
#endif
	}
	Need_Volts = false;
}


void Voltage_Init()
{
	uint32	v, adc;

	STOP_I2C();
	nV = 0;
	sumV = 0;
	AvgV = 0;
	Read_Volts(&v);		//	Read 3 times before using...
	uS_Delay(100000);
	Read_Volts(&v);
	uS_Delay(100000);
	v = 0;
	Read_Volts(&v);
	v = ADC_To_Volts (v);			//	Convert to voltage    LSB = 1mV

	if (v < 7700)	Board_Fails |= VOLT_FAIL;	//	Vbat < 7.7
	if (v > 65000)	Board_Fails |= VOLT_FAIL;	//	Vbat > 65.0

	if (v < 16000)  Battery_Size = 1;		//	Its a 12v battery
	else
	if (v < 30000)  Battery_Size = 2;		//	Its a 24v battery
	else
	if (v < 45000)  Battery_Size = 3;		//	Its a 36v battery
	else
	Battery_Size = 4;						//	Its a 48v battery

	adc = Volts_To_ADC (Battery_Size * MODE_NOMINAL_V);	//	ADC = nominal battery voltage, in A/D counts
	AbsMinV = adc >> 1;						//	A/D reading must be greater than 0.5 * V nominal
	AbsMaxV = adc + AbsMinV;				//	A/D reading must be no more than 1.5 * V nominal
}

