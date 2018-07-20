/*
 * Copyright (c) 2018 Integrated Solutions, Inc.
 * All rights reserved, worldwide.
 *
 */


#include "hal.h"
#include "main.h"
#include "stm32f0xx_gpio.h"
#include "stm32f0xx_rcc.h"
#include "Debug.h"

#define  Temp_Table_Size  140

static int32	sumV;
static int		nV;
static int		AvgV;

const uint16  Temp_Table[Temp_Table_Size] =
{
	50632, 50010, 49376, 48729, 48070, 47400, 46720, 46029, 45329, 44620, 
	43904, 43181, 42451, 41715, 40976, 40232, 39485, 38736, 37986, 37236, 
	36485, 35736, 34989, 34245, 33504, 32768, 32036, 31310, 30590, 29877, 
	29171, 28474, 27784, 27104, 26433, 25772, 25121, 24481, 23851, 23232, 		/*	Index = Temperature (C)	*/
	22625, 22029, 21444, 20871, 20310, 19761, 19224, 18698, 18185, 17683, 		/*	Contents = A/D value	*/
	17193, 16715, 16249, 15794, 15351, 14919, 14498, 14089, 13690, 13302, 		/*	0-139 C					*/
	12924, 12557, 12200, 11852, 11515, 11187, 10868, 10559, 10258,  9967, 
	 9683,  9408,  9141,  8882,  8630,  8386,  8150,  7920,  7697,  7481, 
	 7271,  7068,  6870,  6679,  6494,  6314,  6139,  5970,  5806,  5647, 
	 5492,  5342,  5197,  5056,  4920,  4787,  4659,  4534,  4413,  4296, 
	 4182,  4072,  3965,  3861,  3760,  3663,  3568,  3476,  3386,  3299, 
	 3215,  3133,  3054,  2977,  2902,  2830,  2759,  2690,  2624,  2559, 
	 2496,  2435,  2376,  2319,  2263,  2208,  2155,  2104,  2054,  2005, 
	 1958,  1912,  1868,  1824,  1782,  1741,  1701,  1662,  1624,  1587
};


static void START_I2C()
{
	TEMP_SDA(1);				//	Send repeated-start if necessary
	TEMP_SCL(1);
	uS_Delay(3);
	TEMP_SDA(0);				//	Do START bit
	uS_Delay(3);
	TEMP_SCL(0);
	uS_Delay(5);
}


static void STOP_I2C()
{
	TEMP_SDA(0);
	uS_Delay(3);
	TEMP_SCL(1);
	uS_Delay(3);
	TEMP_SDA(1);
	uS_Delay(5);
}


static void ACK_I2C()			//	ACK ==> continue reading
{
	TEMP_SDA(0);
	TEMP_SCL(1);
	uS_Delay(3);
	TEMP_SCL(0);
	TEMP_SDA(1);				//	allow slave's data to come in again
}


static void NACK_I2C()			//	NACK ==> discontinue read
{
	TEMP_SDA(1);
	uS_Delay(3);
	TEMP_SCL(1);
	uS_Delay(3);
	TEMP_SCL(0);
}


static bool WRITE_I2C_BYTE (int c)
{
	int  n;
	bool b;

	for (n=0; n<8; n++)
	{
		if (c & 0x80)			//	Shift out the data, MSB first...
			TEMP_SDA(1);
		else
			TEMP_SDA(0);

		c <<= 1;
		TEMP_SCL(1);
		uS_Delay(3);
		TEMP_SCL(0);
	}
	TEMP_SDA(1);				//	Set data line HI so ACK can come in
	uS_Delay(3);
	TEMP_SCL(1);				//	Clock in the device's ACK bit
	uS_Delay(5);
	b = true;
	if (TEMP_DAT)
		b = false;
	TEMP_SCL(0);
	return	b;
}


static int READ_I2C_BYTE()
{
	int	n=8;
	int c=0;

	while (--n >= 0)
	{
		c <<= 1;
		if (TEMP_DAT)			//	Shift in the data, MSB first...
			c++;
		TEMP_SCL(1);
		uS_Delay(3);
		TEMP_SCL(0);
	}
	return	c;
}


bool Read_Temperature (uint32 *v)
{
	bool	b;
	uint32  a;
	int		msb, lsb;

	a = 0x29;						//	Perform a read access
	if (Set_Temp_Update_Rate)
		a--;						//	If time to set the update rate, make it a write access

	START_I2C();
	b = WRITE_I2C_BYTE (a);
	if (!b)
	{
		STOP_I2C();					//	A/D still converting: close the bus transaction and exit
		return	false;
	}

	if (Set_Temp_Update_Rate)
	{
		Set_Temp_Update_Rate = false;
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

	a = msb & 0xFF;
	a <<= 8;
	a += lsb & 0xFF;
	*v = a;

	if (a >= Temp_Table[0])
		return	false;				//	Reading too high
	if (a <= Temp_Table[Temp_Table_Size-1])
		return	false;				//	Reading too low

	return	true;					//	Valid reading
}


int  Get_Temperature()				//	OUT:	battery temperature, averaged over previous second.  LSB= 0.1 C
{
	int32  v, t, i;
	int32  a, b;

	if (nV > 0)
		v = sumV / nV;
	else
		v = AvgV;					//	No average available: use last second's value

	AvgV = v;
	sumV = 0;						//	Ready for next average
	nV = 0;

	if ((uint16)v >= Temp_Table[0])
		return	0;					//	Temp is 0 or less

	for (i=1; i<Temp_Table_Size; i++)
		if (Temp_Table[i] <= v)
			break;					//	Temp is between [i-1] and [i]

	if (i >= Temp_Table_Size)
		return	139;				//	Temp is 139 or greater

	a = Temp_Table[i-1];			//	Temp is between i-1 and i  (degrees C)
	b = a - Temp_Table[i];			//	B = distance from [i-1] to [i]
	t = (i-1) * 10;					//	Store the whole-degrees part	LSB = 0.1 C

	i = (((a-v)*10)+(b>>1)) / b;	//	Fractional degrees, rounded off   (tenths)
	t += i;
	return	t;						//	Full temperature	LSB = 0.1 C
}


void Handle_Temp()
{
	uint32  t;

	Need_Temp = false;
	if (Read_Temperature (&t))		//	Ignore process if data not available yet
	{
		sumV += t;					//	Add to the average
		nV++;
#if 0	// TODO: testing for anomaly, remove later
		if (gPOR == 0)
		{
			if (t < 32757 || t > 32760)
				debug_Printf("*** t: %d ***\r\n", t);
		}
#endif
	}
}


void Temperature_Init()
{
	uint32  v;

	STOP_I2C();

	nV = 0;
	sumV = 0;
	AvgV = 0;
	Read_Temperature (&v);		//	Read 3 times...
	uS_Delay(100000);
	Read_Temperature (&v);
	uS_Delay(100000);
	if (!Read_Temperature (&v))
		Board_Fails |= TEMP_FAIL;	//	Reading is out of range on the third try
}

