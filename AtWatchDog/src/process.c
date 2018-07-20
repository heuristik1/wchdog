/*
 * Copyright (c) 2018 Integrated Solutions, Inc.
 * All rights reserved, worldwide.
 *
 */


#include <string.h>
#include "main.h"
#include "volts.h"
#include "temp.h"
#include "flash.h"
#include "process.h"

void process_Init();

Config_t  Config;
int  Current_Volts;			//	0.001 V   at time T=0
int  Last_Logged_Volts;		//  last thing we logged for comparison
int  Last_Volts;			//  last volts monitored (normally 1 second ago)
int  Current_Temp;			//	0.1 C     at time T=0
int  Last_Logged_Temp;		//  last thing we logged for comparison
int  Last_Temp;				//  last temp monitored (normally 1 second ago)
int  V_lwr;
int  V_upr;
int  V_del;
int  V_delx;
int  T_del;
int  T_delx;

// -------------------------------------------------------------------------------

void Handle_VT()					//	Check on Voltage and Temperature averages once per second
{
	int32  v, t, Reason;
	int32  dV;
	int32  dVx;						//	One-second voltage delta
	int32  dT_Slope;
	int32  dT;						//  One-second temp change

	Reason = 0;						//	A reason for logging
	Check_VT = false;

	v = Get_Volts();				//	Get average voltage and restart the averaging for next second
	t = Get_Temperature();			//	Get average temperature and restart the averaging for next second

	dVx = v - Last_Volts;			//	Change in btty voltage over the last second
	dV = v - Last_Logged_Volts;		//	Change in btty voltage since last logged record
	Last_Volts = Current_Volts = v;

	dT_Slope = t - Last_Temp;
	dT = t - Last_Logged_Temp;
	Last_Temp = Current_Temp  = t;

	while (1)							//	Allow BREAKs to act as "goto Exit"
	{
		if (gPOR > 0)					//	Wait for Power On Cycle
		{
			if (--gPOR > 0)
			{
				Tampered = false;		//	POR still active:  Keep tamper flag off
			}

			if (gPOR == 0)					//  POR done?
			{
				Reason = FLAG_STARTUP;		//	POR just ended:  Generate first log since power-on
				process_Init();				//	First time in
			}
			break;
		}

		if (dV > V_del  ||  dV < -V_del)
		{
			Reason = FLAG_DV_SLOPE;
		}

		if (dVx > V_delx  ||  dVx < -V_delx)
		{
			Reason |= FLAG_DVX;
		}

		if (dT_Slope > T_del  ||  dT_Slope < -T_del)
		{
			Reason |= FLAG_DT_SLOPE;
		}

		if (dT > T_delx  ||  dT < -T_delx)
		{
			Reason |= FLAG_DT;
			break;
		}

		if (--Periodic == 0)
		{								//	Generate a log once an hour if all is quiet
			Reason = FLAG_PERIODIC;
			break;
		}

		if (Tampered)
		{								//	Generate a log if Tamper input changed
			Tampered = false;
			Reason = FLAG_TAMPERED;
			break;
		}
		break;							//	Exit now if NOT generating a log point
	}

	if (Reason > 0)
		flash_Log (Reason);				//	Generate a log point, and show why
}


void Config_Callback (Config_t *NewConfig)
{
	Config.Report_Rate         = NewConfig->Report_Rate;
	Config.Periodic_Event_Rate = NewConfig->Periodic_Event_Rate;
}


void Time_Callback (uint32 CurrentEpoch)
{
	if (gPOR > 0)
		flash_Log (FLAG_PERIODIC);				//	Generate a log, just to show time
	TheTime.Epoch = CurrentEpoch;
	if (gPOR > 0)
		flash_Log (FLAG_RESYNC);				//	Generate a log, showing true time
}


uint32 process_ReadEpoch()
{
	return Get_time();						// has mutex
}


void process_Init()
{
	V_lwr = MODE_MIN_V * Battery_Size;		//	Set voltage trip points, based on nominal btty voltage
	V_upr = MODE_MAX_V * Battery_Size;
	V_del = MODE_dV * Battery_Size;
	V_delx = MODE_dVx * Battery_Size;
	T_del = MODE_dT;
	T_delx = MODE_dTx;
	Last_Volts = Current_Volts;
	Last_Temp = Current_Temp;
}


