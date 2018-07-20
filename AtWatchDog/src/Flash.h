/*
 * Copyright (c) 2018 Integrated Solutions, Inc.
 * All rights reserved, worldwide.
 *
 */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __FLASH_H
#define __FLASH_H

#include  "main.h"

#define		EventsPerPage	170			/*	12 B/event   2048 B/page	*/

#define		FLAG_PERIODIC	0x0001
#define		FLAG_DT_SLOPE	0x0002		/*	Temp changed by .5C since last logged event	*/
#define		FLAG_DT			0x0004		/*	Temp changed by .1C or more in one second 	*/
#define		FLAG_DV_SLOPE	0x0008		/*	Vbat changed since last logged event	*/
#define		FLAG_DVX		0x0010		/*	Vbat changed 40mV or more in one second	*/
#define		FLAG_TAMPERED	0x0020
#define		FLAG_STARTUP	0x0040		/*	First event, at POR	*/
#define		FLAG_RESYNC		0x0080		/*	Immediately follows the previous event, but with correct time	*/
#define		FLAG_FLUSH		0x0100

typedef struct
{
	uint32	TimeStamp;			//	Seconds since 1970
	uint16	mVolts;				//	0.001 V
	uint16	dTemp;				//	0.1 C
	uint16	flags;
	uint16	filler;
} Event_t;						//	10 Bytes/event


typedef struct
{
	uint16	BadBlock;			//	FFFF = ok
	uint16	User2;
	uint32	User1;
	uint8	ECC1[6];
	uint8	ECC2[2];
} Spare_t;						//	16 Bytes/spare


typedef struct
{
	Event_t	Events[EventsPerPage];
	uint16	filler;
	uint8	HasTimeUpdt;		//	FF: no time updates in these records
	uint8	Uploaded;			//	FF: not uploaded
	uint32	Sequence;			//	Incrementing value.  0 and -1 not used
	Spare_t	Spare[4];
} Flash_Page_t;					//	The extended page contents   2048+64 bytes


typedef struct
{								//	Page contents starting at offset 2040
	uint16	filler;
	uint8	HasTimeUpdt;
	uint8	Uploaded;			//	FF: not uploaded
	uint32	Sequence;			//	Incrementing value.  0 and -1 not used
	Spare_t	Spare[4];
} Partial_Page_t;				//	The final 72 bytes of extended page contents


typedef struct
{
	Flash_Page_t  Data;			//	One page worth of event data
	uint32	First_Page;
	uint32	Current_Page;
	uint32	Last_Page;
	uint32	Index;				//	Index into Data.Events[]
	bool	Nothing;			//	TRUE if no stored records to be uploaded
} Send_t;


extern Flash_Page_t	ThePage;
extern Send_t	Send;
extern int		nEvents;


extern void Flash_Init();
extern void Begin_Report();
extern void Flash_Disable();
extern void flash_Log (int cause);
extern void Flash_Locate_Latest_Record();
extern int  Report_Callback (Event_t *SomeData);
extern void Successful_Xfer_Callback();



#endif /* __FLASH_H */
