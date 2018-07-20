/*
 * Copyright (c) 2018 Integrated Solutions, Inc.
 * All rights reserved, worldwide.
 *
 */


#include "stm32f0xx_gpio.h"
#include "stm32f0xx_spi.h"
#include <string.h>
#include "process.h"
#include "flash.h"
#include "main.h"
#include "hal.h"
#include "Debug.h"

/* Scheduler include files */
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

//---------------------------------------

#define	TestPageNumber  65408			/*	2nd block from the end		128KB per block		*/

static uint32	Free_Page;				//	Page number of next page to write
static uint32	Sequence;				//	Latest used sequence number
static uint32	UniqueID[8];			//	First word gets the CRC of the original 8 words
static uint32	DummyRead;				//	Dummy read data goes here during DMA Writes

static Flash_Page_t		aPage;			//	Exended page buffer in RAM
static Partial_Page_t	Page_Info;		//	Final 72 bytes of a page

Flash_Page_t  ThePage;					//	Events as they build up, before writing to flash

int		nEvents;						//	Index into ThePage.Events[] when storing events
Send_t	Send;							//	Information for uploading

xSemaphoreHandle	flash_Mutex;

//---------------------------------------

void SPI1_Init()
{
	GPIO_PinAFConfig (GPIOA, 5, 0);		//	Pin A5 = SPI fnc  (SCK)
	GPIO_PinAFConfig (GPIOA, 6, 0);		//	Pin A6 = SPI fnc  (MOSI)
	GPIO_PinAFConfig (GPIOA, 7, 0);		//	Pin A7 = SPI fnc  (MISO)
//	GPIO_PinAFConfig (GPIOA,15, 0);		//	Pin A15= SPI fnc  (-CS)

//	SPI_RxFIFOThresholdConfig (SPI1, SPI_RxFIFOThreshold_HF);

	SPI1->CR1 = 0x0304;					//	Master;  SS pin in software;  MSB first;  no CRC;  CLK = 24MHz/2
	SPI1->CR2 = 0x1700;					//	8-bit data
	SPI1->CR1 |= SPI_CR1_SPE;			//	Enable the SPI unit
}


void SPI_Wait()					//	Polls the SPI unit for BUSY.  Returns when Not Busy
{
	while (SPI1->SR & SPI_SR_BSY)
	  ;
}


void SPI_Tx_Wait()				//	Polls the SPI unit for Tx FIFO Full
{								//	Returns when Tx not full - ready for another byte
	while ((SPI1->SR & 0x1800) == 0x1800)
	  ;
}


void SPI_Rx_Wait()				//	Polls the SPI unit for Tx FIFO Full
{								//	Returns when Tx not full - ready for another byte
	while ((SPI1->SR & 1) == 0)
	  ;
}


uint8 SPI_Write (uint8 byte)
{
	uint8  b;

	SPI_Tx_Wait();				//	Wait for Tx to be Not Full
	SPI_SendData8 (SPI1, byte);
	uS_Delay (3);
	SPI_Rx_Wait();				//	Wait for Rx to have something
	b = SPI_ReceiveData8 (SPI1);
	return	b;
}


uint8 SPI_Write_3 (uint8 b1, uint8 b2, uint8 b3)
{
	uint8  b;

	while (SPI1->SR & 0x1800)	//	Wait for Tx FIFO to be Empty
	  ;
	SPI_SendData8 (SPI1, b1);
	SPI_SendData8 (SPI1, b2);
	SPI_SendData8 (SPI1, b3);

	while ((SPI1->SR & 2)==0)	//	wait here until Tx Empty
		;
	while (SPI1->SR & 1)		//	Read Rx until Empty
		b = SPI_ReceiveData8 (SPI1);
	return	b;					//	Return final byte
}


uint8 SPI_Write_4 (uint8 b1, uint8 b2, uint8 b3, uint8 b4)
{
	uint8  b=0;

	while (SPI1->SR & 0x1800)	//	Wait for Tx FIFO to be Empty
	  ;
	SPI_SendData8 (SPI1, b1);
	SPI_SendData8 (SPI1, b2);
	SPI_SendData8 (SPI1, b3);
	SPI_SendData8 (SPI1, b4);

	while ((SPI1->SR & 2)==0)	//	wait here until Tx Empty
		;
	while (SPI1->SR & 1)		//	Read Rx until Empty
		b += SPI_ReceiveData8 (SPI1);
	return	b;					//	Return dummy byte  (avoid compiler warnings)
}


void Flash_Enable()				//	Asserts the Chip Select line to the FLASH
{
	GPIO_WriteBit (GPIOA, SS_pin, Bit_RESET);	//	Select the FLASH IC   (line LOW)
}


void Flash_Disable()			//	De-asserts the Chip Select line to the FLASH
{
	GPIO_WriteBit (GPIOA, SS_pin, Bit_SET);		//	De-select the FLASH IC   (line HIGH)
}


uint8 Flash_Wait()				//	Polls the Flash IC for BUSY.  Returns when the IC is Not Busy.
{								//	Returns the ECC-Fail, Write-Fail, and Erase-Fail bits.
	uint8  b;

	Flash_Enable();
	b = SPI_Write_3 (5,0xC0,0);	//	Read status register 3...
	while (b & 1)
		b = SPI_Write (0);		//	Read continuously, without breaking the access, until ready
	Flash_Disable();
	return	(b & 0x3C);			//	Return just the failure bits
}


bool DMA_Wait()					//	Polls the DMA engine.  Returns when the full transfer is complete
{
	uint32  f;

	while (1)						//	no timeout
	{								//	Loop here until both Tx and Rx finish, with or without errors
		uS_Delay (30);
		f = DMA1->ISR;				//	Get Interrupt Status Register
		if (f & DMA_ISR_TCIF3)
			if ((f & DMA_ISR_TCIF2) || (f & DMA_ISR_TEIF2))
				break;				//	Read is Complete
		if (f & DMA_ISR_TEIF3)
			break;					//	If Tx error'd out, don't wait for Rx to complete
	}

	DMA1_Channel2->CCR = 0;			//	Disable Rx DMA
	DMA1_Channel3->CCR = 0;			//	Disable Tx DMA

	SPI1->CR2 &= ~SPI_CR2_RXDMAEN;	//	Disable DMA transfer requests for SPI bus
	SPI1->CR2 &= ~SPI_CR2_TXDMAEN;

	if (f & DMA_ISR_TCIF3)
		if (!(f & DMA_ISR_TEIF2))
			return	true;			//	Tx completed, no Rx error

	return false;					//	Error
}


void DMA_Start_Read (uint8 *RamAddr, uint32 Len)	//	Starts the DMA engine in reading a block of data into RAM
{
	DMA1_Channel2->CCR = 0;					//	Make sure DMA is disabled for SPI
	DMA1_Channel3->CCR = 0;
	DMA1->IFCR = DMA_IFCR_CTCIF2 + DMA_IFCR_CTEIF2;		//	Clear the Finished and Error flags
	DMA1->IFCR = DMA_IFCR_CTCIF3 + DMA_IFCR_CTEIF3;		//	  for both Tx and Rx.

	DMA1_Channel2->CPAR = (int)&SPI1->DR;	//	Rx transfer: get the data from here
	DMA1_Channel3->CPAR = (int)&SPI1->DR;	//	Tx transfer: send the data here
	DMA1_Channel2->CMAR = (int)RamAddr;		//	Rx transfer: store the data here
	DMA1_Channel3->CMAR = (int)RamAddr;		//	Tx transfer: fetch the data from here
	DMA1_Channel2->CNDTR = Len;				//	Transfer size
	DMA1_Channel3->CNDTR = Len;

	DMA1_Channel2->CCR  = DMA_CCR_MINC + DMA_CCR_PL_0;					//	Incr memory, not peripheral addr (Rx)
	DMA1_Channel3->CCR  = DMA_CCR_MINC + DMA_CCR_PL_0 + DMA_CCR_DIR;	//	Incr memory, not peripheral addr (Tx)
	DMA1_Channel2->CCR |= DMA_CCR_EN;		//	Enable Rx DMA
	DMA1_Channel3->CCR |= DMA_CCR_EN;		//	Enable Tx DMA
	SPI1->CR2 |= SPI_CR2_RXDMAEN;			//	Enable the SPI Rx requests to DMA engine
	SPI1->CR2 |= SPI_CR2_TXDMAEN;			//	Enable the SPI Tx requests to DMA engine
}


void DMA_Start_Write (uint8 *RamAddr, uint32 Len)		//	Starts the DMA engine in writing a block of data into FLASH
{
	DMA1_Channel2->CCR = 0;					//	Make sure DMA is disabled for SPI
	DMA1_Channel3->CCR = 0;
	DMA1->IFCR = DMA_IFCR_CTCIF2 + DMA_IFCR_CTEIF2;		//	Clear the Finished and Error flags
	DMA1->IFCR = DMA_IFCR_CTCIF3 + DMA_IFCR_CTEIF3;		//	  for both Tx and Rx.

	DMA1_Channel2->CPAR = (int)&SPI1->DR;	//	Rx transfer: get the data from here
	DMA1_Channel3->CPAR = (int)&SPI1->DR;	//	Tx transfer: send the data here
	DMA1_Channel2->CMAR = (int)&DummyRead;	//	Rx transfer: store the data here
	DMA1_Channel3->CMAR = (int)RamAddr;		//	Tx transfer: fetch the data from here
	DMA1_Channel2->CNDTR = Len;				//	Transfer size
	DMA1_Channel3->CNDTR = Len;

	DMA1_Channel2->CCR  = DMA_CCR_PL_0;		//	No incr of RAM or Peripheral address!	(Rx)
	DMA1_Channel3->CCR  = DMA_CCR_PL_0 + DMA_CCR_MINC + DMA_CCR_DIR;	//	Incr memory, not peripheral addr (Tx)
	DMA1_Channel2->CCR |= DMA_CCR_EN;		//	Enable Rx DMA
	DMA1_Channel3->CCR |= DMA_CCR_EN;		//	Enable Tx DMA

	SPI1->CR2 |= SPI_CR2_RXDMAEN;			//	Enable the SPI Rx requests to DMA engine
	SPI1->CR2 |= SPI_CR2_TXDMAEN;			//	Enable the SPI Tx requests to DMA engine
}


void Write_Status_Register (int reg, uint8 val)		//	REG is 1,2,3		VAL is byte to write
{
	uint8 a = (reg<<4) + 0x90;		//	Status register address   (0xA0, 0xB0, 0xC0)
	Flash_Wait();					//	Make sure Flash is idle
	Flash_Enable();
	SPI_Write_3 (1, a, val);		//	write status
	Flash_Disable();
}


uint8 Read_Status_Register (int reg)	//	REG is 1,2,3
{
	uint8  b;
	uint8  a = (reg<<4) + 0x90;		//	Status register address   (0xA0, 0xB0, 0xC0)

	Flash_Wait();					//	Make sure Flash is idle
	Flash_Enable();
	b = SPI_Write_3 (5, a, 0);		//	read status
	Flash_Disable();
	return	b;
}


bool Flash_Read_Page_Info (int page, Partial_Page_t *pp)	//	page = 0-64K	(2KByte per page)
{															//	Ptr  = RAM-based buffer area
	uint8  b, msb, lsb;										//	OUT:	FALSE if errors in page data
	uint8  *ptr;

	msb = (page >> 8) & 0xFF;
	lsb = page & 0xFF;
	ptr = (uint8 *)pp;

	Flash_Wait();					//	Make sure Flash is idle
	Flash_Enable();
	SPI_Write_4 (0x13,0,msb,lsb);	//	PAGE DATA READ...
	Flash_Disable();

	b = Flash_Wait();				//	Wait for internal read of the page
	if (b & 0x20)
		return	false;				//	Had Errors in the page

	Flash_Enable();
	SPI_Write_4 (3,0x07,0xF8,0);	//	READ...		offset (msb,lsb), dummy

	DMA_Start_Read (ptr, 72);		//	Start the DMA: read final 72 bytes of the extended page
	DMA_Wait();						//	Wait for DMA complete
	Flash_Disable();
	return	true;
}

										//	Erase 128KBytes...
bool Flash_Erase_Block (int page)		//	page = 0-64K   (low 6 bits = 0.)
{										//			64 pages erased, 2KByte per page, 128KB total
	uint8  b, msb, lsb;					//	OUT:	FALSE if error

	msb = (page >> 8) & 0xFF;
	lsb = page & 0xC0;				//	Bottom 6 bits zeroed

	Flash_Wait();
	Flash_Enable();
	SPI_Write (0x06);				//	WRITE ENABLE...
	Flash_Disable();

	Flash_Wait();					//	Make sure Flash is idle
	Flash_Enable();
	SPI_Write_4 (0xD8,0,msb,lsb);	//	ERASE 1 BLOCK:  64 PAGES...
	Flash_Disable();
	b = Flash_Wait();				//	Wait for internal erase

	if (b & 0x0C)
		return	false;				//	Had Errors
	return	true;
}


bool Flash_Read_Page (int page, Flash_Page_t *pp)	//	page = 0-64K	(2KByte per page)
{													//	ptr  = 2048 Byte buffer area
	uint8  b, msb, lsb;
	uint8  *ptr;

	msb = (page >> 8) & 0xFF;
	lsb = page & 0xFF;
	ptr = (uint8 *)pp;

	Flash_Wait();					//	Make sure Flash is idle
	Flash_Enable();
	SPI_Write_4 (0x13,0,msb,lsb);	//	PAGE DATA READ...
	Flash_Disable();

	b = Flash_Wait();				//	Wait for internal read of the page
	if (b & 0x20)
		return	false;				//	Had Errors in the page

	Flash_Enable();
	SPI_Write_4 (3, 0, 0, 0);		//	READ...		offset = 0
	DMA_Start_Read (ptr, 2048);		//	Start the DMA: read the 2048 bytes   (takes 1.36 mS)
	DMA_Wait();						//	Wait for DMA complete
	Flash_Disable();
	return	true;
}


bool Flash_Write_Page (int page)
{										//	page = 0-64K
	uint8  b, msb, lsb;					//	ThePage = 2048 Byte buffer area - also writes the 64 extra bytes
	uint8  *ptr;
										//	OUT:	FALSE if error
	msb = (page >> 8) & 0xFF;
	lsb = page & 0xFF;
	ptr = (uint8 *)&ThePage;

	Flash_Wait();
	Flash_Enable();
	SPI_Write (0x06);				//	WRITE ENABLE...
	Flash_Disable();

	Flash_Wait();
	Flash_Enable();
	SPI_Write_3 (2, 0, 0);			//	PAGE LOAD DATA...		offset = 0
	DMA_Start_Write (ptr, 2048+64);
	DMA_Wait();						//	Wait for DMA complete
	Flash_Disable();

	Flash_Wait();
	Flash_Enable();
	SPI_Write_4 (0x10,0,msb,lsb);	//	PROGRAM EXECUTE...   (write page to flash array)
	Flash_Disable();				//	Begin the write

	b = Flash_Wait();				//	Wait for internal write
	if (b & 0x0C)
		return	false;				//	Had Errors

	b = Flash_Read_Page (page, &aPage);
	if (!b)
		return	false;				//	Could not read it back

	if (memcmp (&ThePage, &aPage, 2048))
		return	false;				//	Did not match

	return	true;					//	Read back perfectly!
}


bool Flash_Page_OK (int page)				//	IN:  page = 0-64K	(2KByte per page)
{											//	OUT: Page_Info
	uint16	OK = 0xFFFF;

	if (page == TestPageNumber)		//	don't use the test page
		return	false;

	if (Flash_Read_Page_Info (page, &Page_Info))
		if (Page_Info.Spare[0].BadBlock == OK)
			if (Page_Info.Spare[1].BadBlock == OK)
				if (Page_Info.Spare[2].BadBlock == OK)
					if (Page_Info.Spare[3].BadBlock == OK)
						return	true;
	return	false;							//	 Returns TRUE if no ECC errors, and the BadBlock markers are OK
}


void Flash_Read_Unique_ID()				//	Out:  aPage <-- 32 bytes of unique ID as read from the FLASH IC
{
	int  i;

	Write_Status_Register(2,0x58);		//	OTP-E = 1		ECC-E = 1		BUF = 1
	Flash_Wait();
	Flash_Enable();
	SPI_Write_4 (0x13,0,0,0);			//	PAGE DATA READ...		page number 0
	Flash_Disable();

	Flash_Wait();
	Flash_Enable();
	SPI_Write_4 (3,0,0,0);					//	READ...		offset = 0
	DMA_Start_Read ((uint8 *)UniqueID,32);	//	Start DMA:  read the 32 ID bytes
	DMA_Wait();								//	Wait for DMA complete
	Flash_Disable();
	Write_Status_Register(2,0x18);		//	OTP-E = 0		ECC-E = 1		BUF = 1

	CRC->CR = 1;						//	Begin a new CRC...
	for (i=0; i<8; i++)
		CRC->DR = UniqueID[i];
	UniqueID[0] = CRC->DR;				//	Replace the first word with the CRC of all 8 words
	CRC->CR = 1;
}


void Flash_Locate_Next_Page()			//	Out:  Free_Page = next page to write
{										//		  Sequence  = next Sequence number to use
	bool erased = false;
	int  i;

	if (++Free_Page == 65536)			//	Begin search with next page
		Free_Page = 0;

	for (i=0; i<65536; i++)				//	Try all pages once, then quit
	{
		if (Flash_Page_OK (Free_Page))
		{
			if (Page_Info.Sequence == 0xFFFFFFFF)
				break;							//	Found a good page to try

			if ((Free_Page & 0x3F)==0 && !erased)		///	TODO: fix: what about if first good page in a block is not the first page
			{
				Write_Status_Register(1,0);		//	Enable FLASH writes to all pages
				Flash_Erase_Block (Free_Page);	//	Erase a block if at the beginning
				Write_Status_Register(1,0x7C);	//	  and it hasn't been erased yet.
				erased = true;
				i--;
				continue;
			}
		}
		erased = false;

		if (++Free_Page >= 65536)
		{
			Free_Page = 0;
			Write_Status_Register(1,0);		//	Enable FLASH writes to all pages
			Flash_Erase_Block(0);			//	Rolled over to first page: start clearing old data
			Write_Status_Register(1,0x7C);	//	Disable FLASH writes
			erased = true;
		}
	}
}

	//	This is called at POR to find the next free page to write,
	//		and the span of records to be uploaded.
	//
void Flash_Locate_Latest_Record()			//	Out:  Free_Page  = next page to write
{											//		  Sequence   = next Sequence number to use
	int32	diff;							//		  First_Page = first page requiring upload
	uint32	i, seq;							//		  Last_Page  = final page requiring upload
	uint32	LoSeq = 0;						//		  Takes 15 seconds
	uint32	HiSeq = 0;

	Send.Nothing = true;					//	Nothing to send (yet)
	Free_Page = 0;
	Sequence = 1;

	for (i=0; i<65536; i++)					//	Search every page for first valid sequence number...
	{
		if (! Flash_Page_OK(i))
			continue;						//	Skip unreadable pages

		if (1+Page_Info.Sequence == 0)		//	Sequence = all F's ?
			continue;						//	  Yes.  Un-written page - skip it

		Sequence = Page_Info.Sequence;		//	Record first sequence # so that the DIFFing in the next loop works,
		Free_Page = i;						//	  no matter what the first unsigned sequence value is.
		break;
	}

	if (i == 65536)							//	If a totally blank Flash chip, exit now.
		return;

	--i;									//	Decrement so we re-locate the first valid page again
	while (++i < 65536)						//	Search every page...
	{
		if (Flash_Page_OK(i))
		{
			seq = Page_Info.Sequence;
			if (seq == 0xFFFFFFFF)
				continue;					//	Un-written page - ignore it

			if (Page_Info.Uploaded == 0xFF)
			{								//	This page needs uploading:  Data is good, Sequence is valid, Uploaded it is not
				if (Send.Nothing)
				{							//	This is the first page found to upload
					LoSeq = seq;
					HiSeq = seq;
					Send.Last_Page = i;
					Send.First_Page = i;
					Send.Nothing = false;	//	We have something
				}
				else
				{
					diff = seq - LoSeq;
					if (diff < 0)
					{
						LoSeq = seq;		//	Mark new first page  (happens when upload pages wrapped around)
						Send.First_Page = i;
					}
					diff = seq - HiSeq;
					if (diff > 0)
					{
						HiSeq = seq;		//	Mark new last page
						Send.Last_Page = i;
					}
				}
			}

			diff = seq - Sequence;
			if (diff > 0)
			{
				Sequence = seq;				//	Keep track of highest sequence number found and
				Free_Page = i;				//		its location, whether it is uploaded or not.
			}
		}
	}

	Flash_Locate_Next_Page();				//	Set Free_Page for first write
	if (++Sequence == 0xFFFFFFFF)			//	Set sequence number for first write
		Sequence = 1;						//	Skip values of -1 and 0
}


bool Flash_Mark_Page_Sent (int page)	//	This marks one page which was sent to the cloud as being
{										//	 successfully uploaded by writing the 'Uploaded' byte in 
	uint8  msb, lsb;					//	 the page structure.
	uint8  a, b;
	int    offs;

	msb = (page >> 8) & 0xFF;
	lsb = page & 0xFF;

	Flash_Wait();					//	Get the page into Flash Device's internal RAM area...
	Flash_Enable();
	SPI_Write_4 (0x13,0,msb,lsb);	//	PAGE DATA READ...
	Flash_Disable();

	Flash_Wait();
	Flash_Enable();
	SPI_Write (0x06);				//	WRITE ENABLE...
	Flash_Disable();

	offs = offsetof(Flash_Page_t, Uploaded);
	a = (offs >> 8) & 0x0F;
	b = offs & 0xFF;

	Flash_Wait();					//	Write a '00' byte to the UPLOADED structure entry...
	Flash_Enable();
	SPI_Write_4 (0x84,a,b,0);		//	RANDOM PAGE LOAD DATA   (does not affect current page data)
	Flash_Disable();

	Flash_Wait();
	Flash_Enable();
	SPI_Write_4 (0x10,0,msb,lsb);	//	PROGRAM EXECUTE...   (write page to flash array)
	Flash_Disable();				//	Begin the write
	b = Flash_Wait();				//	Wait for internal write

	if (b & 0x0C)
		return	false;				//	Had Errors
	return	true;
}


void Flash_Mark_Page_Bad (int page)		//	This marks the page as being unusable by writing to the BadBlock bytes
{
	uint8  msb, lsb;
	uint8  *ptr;

	msb = (page >> 8) & 0xFF;
	lsb = page & 0xFF;
	ptr = (uint8 *)&Page_Info;
	memset (ptr, 0, 64);

	Flash_Wait();
	Flash_Enable();
	SPI_Write (0x06);				//	WRITE ENABLE...
	Flash_Disable();

	Flash_Wait();
	Flash_Enable();
	SPI_Write_3 (2, 8, 0);			//	PAGE LOAD DATA...		offset = 0800h = 2K		point to EXTRA section
	DMA_Start_Write (ptr, 64);		//	Write 64 bytes of zeros in EXTRA section
	DMA_Wait();						//	Wait for DMA complete
	Flash_Disable();

	Flash_Wait();
	Flash_Enable();
	SPI_Write_4 (0x10,0,msb,lsb);	//	PROGRAM EXECUTE...   (write page to flash array)
	Flash_Disable();				//	Begin the write
	Flash_Wait();					//	Wait for internal write - ignore errors
}


void Flash_Write_Events()
{
	int  i;

	ThePage.Sequence = Sequence;		//	Store the sequence number to use
	if (++Sequence == 0xFFFFFFFF)		//	Set sequence number for next time
		Sequence = 1;					//	Skip values of -1 and 0

	if (Send.Nothing)
	{
		Send.First_Page = Free_Page;
		Send.Nothing = false;			//	We have something
	}

	Write_Status_Register (1,0);		//	Enable FLASH writes to all pages
	for (i=0; i<25; i++)				//	Try 25 times to write it
	{
		if (Flash_Write_Page (Free_Page))
			break;						//	Worked!
		Flash_Mark_Page_Bad (Free_Page);
		Flash_Locate_Next_Page();		//	Find another page to try
	}
	Write_Status_Register (1,0x7C);		//	Disable all FLASH writes -- write protected
	Send.Last_Page = Free_Page;			//	Keep track of final page to be uploaded

	if (i < 25)
		Flash_Locate_Next_Page();		//	Find next available page for next time

	memset (&ThePage, 0xFF, sizeof(ThePage));
	nEvents = 0;						//	Clear the buffer area
}


void Begin_Report()
{
	flash_Log (FLAG_FLUSH);					//	Generate a log, and flush the record to FLASH

	xSemaphoreTake(flash_Mutex,portMAX_DELAY);	//  lock it
	Send.Index = 0;							//	Start with first event of first page
	Send.Current_Page = Send.First_Page;
	bool b = Send.Nothing;

	while (!b)								//	While looking for valid data to send...
	{
		b = Flash_Read_Page (Send.Current_Page, &Send.Data);
		if (!b)								//	Fail...
		{									//	Try next page if Current is unreadable
			if (Send.Current_Page == Send.Last_Page)
				break;						//	Stop looping if Last page is unreadable
			Send.Current_Page = (1 + Send.Current_Page) & 0xFFFF;
		}
	}

	if (!b)
		Send.Nothing = true;				//	Nothing to send: all records are unreadable
	xSemaphoreGive(flash_Mutex);			//  release it
}


int  Report_Callback (Event_t *OneEvent)	//	OUT:	 0: OneEvent points to valid event
{											//			-1: no more data
	bool  b;								//			Send.Index is updated for next call

	if (Send.Nothing)
		return	-1;							//	Quit immediately if no records to upload

	xSemaphoreTake(flash_Mutex,portMAX_DELAY);		//  lock it
	while (1)
	{
		if (Send.Index < EventsPerPage)
		{											//	Copy the data into caller's memory - may or may not be valid data
			*OneEvent = Send.Data.Events[Send.Index];
			if (OneEvent->TimeStamp != 0xFFFFFFFF)	//	This entry was valid...
			{
				Send.Index++;						//	Update index for next call
				xSemaphoreGive(flash_Mutex);		//  release it
				return	0;							//	PASS: page is not exhausted and data is valid
			}
		}
	
		if (Send.Current_Page == Send.Last_Page)
		{
			Send.Nothing = true;
			xSemaphoreGive(flash_Mutex);			//  release it
			return	-1;								//	STOP: no more pages to read out
		}
													//	Advance to next page
		Send.Current_Page = (1 + Send.Current_Page) & 0xFFFF;
		Send.Index = 0;								//	Start with first event of first page

		b = false;
		while (!b)									//	Find next readable page...
		{
			b = Flash_Read_Page (Send.Current_Page, &Send.Data);
			if ((!b) || (Send.Data.Uploaded != 0xFF))
			{										//	Current Page is unreadable or uploaded: skip over it
				if (Send.Current_Page == Send.Last_Page)
				{
					Send.Nothing = true;
					xSemaphoreGive(flash_Mutex);	//  release it
					return	-1;						//	STOP: nothing more to upload due to unreadable
				}
				Send.Current_Page = (1 + Send.Current_Page) & 0xFFFF;
			}
		}
	}
}


void Successful_Xfer_Callback()						//	All pages fully transferred, so far, during this 
{													//	  connection are marked as 'sent'.
	xSemaphoreTake(flash_Mutex,portMAX_DELAY);
	Write_Status_Register (1, 0);					//	Enable FLASH writes to all pages

	while (Send.First_Page < Send.Current_Page)
	{
		Flash_Mark_Page_Sent (Send.First_Page);		//	Mark that page as done
		Send.First_Page++;
	}

	if (Send.Nothing == true)
	{
		Flash_Mark_Page_Sent (Send.Last_Page);		//	Mark final page as done
	}

	Write_Status_Register (1, 0x7C);				//	Disable all FLASH writes -- write protected
	xSemaphoreGive(flash_Mutex);
}


void flash_Log (int cause)
{
	Event_t  Event;

	xSemaphoreTake(flash_Mutex,portMAX_DELAY);	//  lock it
	Periodic = Config.Periodic_Event_Rate;		//	Force another event in x seconds, if nothing else happens

	Event.TimeStamp = TheTime.Epoch;		//	Store time, Volts, and Temp of right now
	Event.mVolts = Current_Volts;			//	0.001 V
	Event.dTemp  = Current_Temp;			//	0.1 C

	Last_Logged_Volts = Event.mVolts;
	Last_Logged_Temp = Event.dTemp;

	Event.flags = cause;
	ThePage.Events[nEvents] = Event;

	if ((++nEvents >= EventsPerPage) || (cause == FLAG_FLUSH))
		Flash_Write_Events();
	xSemaphoreGive(flash_Mutex);			// give it back
#if 1
	debug_Printf("**ts:%d, v:%d, t:%d, f:%04x\r\n", Event.TimeStamp,
												  Event.mVolts,
												  Event.dTemp,
												  Event.flags);
#endif
}


static void Flash_Test()				//	Performs a quick test of the FLASH device at POR
{
	int  i;
	uint8  *ptr;
	ptr = (uint8 *)&ThePage;

	for (i=0; i<2048; i++)					//	'ThePage' gets generated test data
		*ptr++ = 1+i;						//	 'aPage' gets Flash contents

	if (Flash_Read_Page (TestPageNumber, &aPage))
		if (!memcmp (&ThePage, &aPage, 2048))
			return;							//	ThePage == aPage:  Flash already has the test data, and works

	Write_Status_Register (1, 0);			//	Enable FLASH writes to all pages

	Flash_Erase_Block (TestPageNumber);		//	Erase the block containing the test page
											//	Note: ThePage still has the test data in it
	ThePage.Spare[0].BadBlock = 0;
	ThePage.Spare[1].BadBlock = 0;			//	Mark the Test Page as no good for normal use
	ThePage.Spare[2].BadBlock = 0;
	ThePage.Spare[3].BadBlock = 0;

	if (! Flash_Write_Page (TestPageNumber))
		Board_Fails |= FLASH_FAIL;			//	Performs write of ThePage, read-back into aPage, and comparison

	Write_Status_Register (1, 0x7C);		//	Disable all FLASH writes -- write protected
}


void Flash_Init()
{
	SPI1_Init();			//	Interface to external flash memory
	Flash_Wait();			//	Make sure Flash is idle
	Flash_Enable();
	SPI_Write (0xFF);		//	Perform a DEVICE RESET of the Flash
	Flash_Disable();
	Flash_Read_Unique_ID();
	Flash_Test();

	memset (&ThePage, 0xFF, sizeof(ThePage));
	nEvents = 0;
	Send.Nothing = true;
	flash_Mutex = xSemaphoreCreateMutex(); // initialize the mutex for access to flash
}
