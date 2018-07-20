/*
 * Copyright (c) 2018 Integrated Solutions, Inc.
 * All rights reserved, worldwide.
 *
 */

#ifndef PROCESS_H_INCLUDED
#define PROCESS_H_INCLUDED

#include "main.h"

#define  MODE_MIN_V		11400	/*	11.4v	*/
#define  MODE_NOMINAL_V	12600	/*  12.6v   */
#define  MODE_MAX_V		13400	/*	13.4v	*/
#define  MODE_dV		200		/*	200 mV from last logged event	*/
#define  MODE_dVx		40		/*  40 mV in last second*/
#define  MODE_dT		5		/*	.5 from last logged event	*/
#define  MODE_dTx		1		/*  .1 in last second*/

typedef struct
{
	int		Periodic_Event_Rate;
	int		Report_Rate;
} Config_t;

extern Config_t  Config;
extern int  Current_Volts;			//	LSB = 0.001 V
extern int  Current_Temp;			//	LSB = 0.1 C
extern int  Last_Logged_Volts;			//	LSB = 0.001 V
extern int  Last_Logged_Temp;			//	LSB = 0.001 V

extern void Handle_VT();
extern void Log (int cause);
extern void Time_Callback (uint32 CurrentEpoch);
extern uint32 process_ReadEpoch();

#endif
