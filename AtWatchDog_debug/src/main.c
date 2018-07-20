/*
 * Copyright (c) 2018 Integrated Solutions, Inc.
 * All rights reserved, worldwide.
 *
 */

/**
  ******************************************************************************
  * @file		main.c
  * @author		Roger Jackson
  * @version	V1.0.0
  * @date		16-Feb-2018
  * @brief		Main program body
  ******************************************************************************
  *
  * Processor:	STM32F030CC
  * ROM:		256K
  * RAM:		32K
  * Freq:		XTAL: 12 MHz	CPU: 24 MHz		Peripherals: 24 MHz
  *				SYSCLK = HCLK = PCLK = 24 MHz
  *				I2C1 = I2C2 = 24 MHz clock source
  * SysTick:	1 mS  (clock = HCLK/8 = 3 MHz)
  *
  *
  ******************************************************************************
  */

/*
    FreeRTOS V7.1.1 - Copyright (C) 2012 Real Time Engineers Ltd.
	

    ***************************************************************************
     *                                                                       *
     *    FreeRTOS tutorial books are available in pdf and paperback.        *
     *    Complete, revised, and edited pdf reference manuals are also       *
     *    available.                                                         *
     *                                                                       *
     *    Purchasing FreeRTOS documentation will not only help you, by       *
     *    ensuring you get running as quickly as possible and with an        *
     *    in-depth knowledge of how to use FreeRTOS, it will also help       *
     *    the FreeRTOS project to continue with its mission of providing     *
     *    professional grade, cross platform, de facto standard solutions    *
     *    for microcontrollers - completely free of charge!                  *
     *                                                                       *
     *    >>> See http://www.FreeRTOS.org/Documentation for details. <<<     *
     *                                                                       *
     *    Thank you for using FreeRTOS, and thank you for your support!      *
     *                                                                       *
    ***************************************************************************


    This file is part of the FreeRTOS distribution.

    FreeRTOS is free software; you can redistribute it and/or modify it under
    the terms of the GNU General Public License (version 2) as published by the
    Free Software Foundation AND MODIFIED BY the FreeRTOS exception.
    >>>NOTE<<< The modification to the GPL is included to allow you to
    distribute a combined work that includes FreeRTOS without being obliged to
    provide the source code for proprietary components outside of the FreeRTOS
    kernel.  FreeRTOS is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
    or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
    more details. You should have received a copy of the GNU General Public
    License and the FreeRTOS license exception along with FreeRTOS; if not it
    can be viewed here: http://www.freertos.org/a00114.html and also obtained
    by writing to Richard Barry, contact details for whom are available on the
    FreeRTOS WEB site.

    1 tab == 4 spaces!
    
    ***************************************************************************
     *                                                                       *
     *    Having a problem?  Start by reading the FAQ "My application does   *
     *    not run, what could be wrong?                                      *
     *                                                                       *
     *    http://www.FreeRTOS.org/FAQHelp.html                               *
     *                                                                       *
    ***************************************************************************

    
    http://www.FreeRTOS.org - Documentation, training, latest information, 
    license and contact details.
    
    http://www.FreeRTOS.org/plus - A selection of FreeRTOS ecosystem products,
    including FreeRTOS+Trace - an indispensable productivity tool.

    Real Time Engineers ltd license FreeRTOS to High Integrity Systems, who sell 
    the code with commercial support, indemnification, and middleware, under 
    the OpenRTOS brand: http://www.OpenRTOS.com.  High Integrity Systems also
    provide a safety engineered and independently SIL3 certified version under 
    the SafeRTOS brand: http://www.SafeRTOS.com.
*/

/*
FreeRTOS is a market leading RTOS from Real Time Engineers Ltd. that supports
31 architectures and receives 77500 downloads a year. It is professionally
developed, strictly quality controlled, robust, supported, and free to use in
commercial products without any requirement to expose your proprietary source
code.

*/

/* Standard includes. */
#include <stdint.h>
#include <string.h>
//#include "stm32f0xx_gpio.h"
//#include "stm32f0xx_rcc.h"
#include "stm32f0xx.h"
#include "LED.h"
#include "hal.h"
#include "main.h"
#include "Temp.h"
#include "Flash.h"
#include "Volts.h"
#include "Debug.h"
#include "Modem.h"
#include "process.h"
//#include "uart1.h"
//#include "uart5.h"
#include "gsmtask.h"
#include "version.h"

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "semphr.h"

/* Private variables ---------------------------------------------------------*/

int		gPOR;
int		Periodic;				//	counts seconds to next forced log
int		Board_Fails;			//	bit per subsystem
int		Battery_Size;			//	1,2,3,4 = number of 12v packs in the battery
int		Current_Mode;			//	0,1,2   = current battery mode
bool	Set_Volt_Update_Rate;
bool	Set_Temp_Update_Rate;
bool	Need_Volts;
bool	Need_Temp;
bool	NewSecond;
bool	Tampered;
bool	Check_VT;

xSemaphoreHandle	epoch_Mutex;

TheTime_t		TheTime;		//	Current time
int g_modemState = 0;

/* Private function prototypes -----------------------------------------------*/
void Do_Time();
void Vars_Init();


/* Priorities at which the tasks are created.  The event semaphore task is
given the maximum priority of ( configMAX_PRIORITIES - 1 ) to ensure it runs as
soon as the semaphore is given. */
#define MAIN_TASK_PRIORITY		( tskIDLE_PRIORITY + 2 )
#define	CELL_TASK_PRIORITY		( tskIDLE_PRIORITY + 1 )

/*-----------------------------------------------------------*/
xSemaphoreHandle	connect_Semaphore;

/*
 * TODO: Implement this function for any hardware specific clock configuration
 * that was not already performed before main() was called.
 */
static void prvSetupHardware( void );

/*
 * The queue send and receive tasks as described in the comments at the top of
 * this file.
 */
static void prvMainTask( void *pvParameters );

/*
 * The callback function assigned to the example software timer as described at
 * the top of this file.
 */
// static void vExampleTimerCallback( xTimerHandle xTimer );

/*-----------------------------------------------------------*/

/* The counters used by the various examples.  The usage is described in the
 * comments at the top of this file.
 */
static volatile uint32_t ulCountOfTimerCallbackExecutions = 0;

/*-----------------------------------------------------------*/

int main(void)
{
	/* Configure the system ready to run the demo.  The clock configuration
	can be done here if it was not done before main() was called. */
	prvSetupHardware();
	Hardware_Init();
	Vars_Init();
	uS_Delay (500000);			//	1/2 second delay before accessing FLASH
	Temperature_Init();
	Voltage_Init();
	debug_Init();
	Modem_Init();		///	TODO: ADD MODEM TEST FOR BOARD-FAILS
	uS_Delay (500000);			//	1/2 second delay before accessing FLASH
	Flash_Init();

	vSemaphoreCreateBinary(connect_Semaphore);
	epoch_Mutex = xSemaphoreCreateMutex(); // initialize the mutex for access to epoch

	/* Create the main task */
	xTaskCreate( 	prvMainTask,					/* The function that implements the task. */
					( signed char * ) "Main", 		/* Text name for the task, just to help debugging. */
					300,							/* The size (in words) of the stack that should be created for the task. */
					NULL, 							/* A parameter that can be passed into the task.  Not used in this simple demo. */
					MAIN_TASK_PRIORITY,/* The priority to assign to the task.  tskIDLE_PRIORITY (which is 0) is the lowest priority.  configMAX_PRIORITIES - 1 is the highest priority. */
					NULL );							/* Used to obtain a handle to the created task.  Not used in this simple demo, so set to NULL. */


	/* Create the queue send task in exactly the same way.  Again, this is
	described in the comments at the top of the file. */
	xTaskCreate( 	prvCellTask,
					( signed char * ) "Cell",
					500,
					NULL,
					CELL_TASK_PRIORITY,
					NULL );

	/* Start the tasks and timer running. */
	vTaskStartScheduler();

	/* If all is well, the scheduler will now be running, and the following line
	will never be reached.  If the following line does execute, then there was
	insufficient FreeRTOS heap memory available for the idle and/or timer tasks
	to be created.  See the memory management section on the FreeRTOS web site
	for more details.  http://www.freertos.org/a00111.html */
	for( ;; );
}

/*-----------------------------------------------------------*/

static void Show_Board_Failure()		//	Does not return!
{
	debug_PutLine("\r\nBoard Fail: ");
	if (Board_Fails & TEMP_FAIL)	debug_PutLine("\tTemp Module");
	if (Board_Fails & VOLT_FAIL)	debug_PutLine("\tVolt Module");
	if (Board_Fails & FLASH_FAIL)	debug_PutLine("\tFlash Module");
	if (Board_Fails & DEBUG_FAIL)	debug_PutLine("\tDebug Module");
	if (Board_Fails & CELL_FAIL)	debug_PutLine("\tCell Module");
	LED_OFF (BLUE_LED);
	LED_OFF (GREEN_LED);
	LED_ON  (RED_LED);
	while (1) ;				//	Wait till watchdog timer resets us, and try again
}

void WaitForModemFinalReady()
{
    // Wait until other task sets modemState to 2.
    while (1)
    {
        vTaskDelay(MS_TO_TICKS(1000));
        if (g_modemState == 2)
        {
            break;
        }
    }
}

/*-----------------------------------------------------------*/
static void prvMainTask( void *pvParameters )
{
	portTickType xDelayTime = 1000 / portTICK_RATE_MS;
	portTickType now = xTaskGetTickCount();
	portTickType lastTick = now;
	portTickType compareTo = MS_TO_TICKS(120 * 1000);	// first post at 2 minutes

	xSemaphoreTake(connect_Semaphore,MS_TO_TICKS(1));

	debug_Printf("EAI WatchDog - Version: %s\r\n",VERSION);
	MODEM_PWRKEY(0);		// modem on button (HIGH)
	if (Board_Fails)
		Show_Board_Failure();		//	Does not return!

	debug_PutLine("Locating latest record...");
	Flash_Locate_Latest_Record();

	// start up the modem the way it wants to
	debug_PutLine("Powering up Modem...");
	MODEM_PWR_EN(1);				// power on Modem
	vTaskDelay(xDelayTime);
	debug_PutLine("Button press");
	MODEM_PWRKEY(1);				// modem on button (LOW) - minimum 1 second
	vTaskDelay(xDelayTime);
	debug_PutLine("Button release");
	MODEM_PWRKEY(0);				// modem on button (HIGH)
	if (Board_Fails)				// Check for late-arriving failures
		Show_Board_Failure();		// Does not return!

    // signal modem thread we are ready to rock
    g_modemState = 1;
    WaitForModemFinalReady();

	for (;;)
	{
		vTaskDelay(1);
		Do_Time();

		if (!gHoldSamplesDuringModemActivity)
		{
			if (Check_VT)
				{
				Handle_VT();			//	Check V & T values once per second
#if 1
				debug_Printf("ts:%d, v:%d, t:%d\r\n", TheTime.Epoch,
															  Current_Volts,
															  Current_Temp);
#endif
				}

			if (Need_Volts)
				Handle_Volts();

			if (Need_Temp)
				Handle_Temp();
		}

		now = xTaskGetTickCount();
		if (now - lastTick > compareTo)
		{
			compareTo = MS_TO_TICKS(Config.Report_Rate * 1000);
			lastTick = now;

			// hold off samples
			gHoldSamplesDuringModemActivity = true;

			xSemaphoreGive(connect_Semaphore);
		}
	}
}

/*-----------------------------------------------------------*/

void vApplicationTickHook( void )
{
	static int	Last_Tamper=0;

	if (NewSecond) {
		NewSecond = false;
		TheTime.Ticks -= 999;			//	Count real-time, and subtract 1000mS
	}
	else
		TheTime.Ticks++;				//	Count real-time

	int Tamper = (TAMPER_IN);			//	Tamper = 0 or (1<<13), depending on input state
	if (Last_Tamper != Tamper)
	{
		Tampered = true;				//	Set flag if Tamper changed
		Last_Tamper = Tamper;
	}
}

/*-----------------------------------------------------------*/

void vApplicationMallocFailedHook( void )
{
	/* The malloc failed hook is enabled by setting
	configUSE_MALLOC_FAILED_HOOK to 1 in FreeRTOSConfig.h.

	Called if a call to pvPortMalloc() fails because there is insufficient
	free memory available in the FreeRTOS heap.  pvPortMalloc() is called
	internally by FreeRTOS API functions that create tasks, queues, software 
	timers, and semaphores.  The size of the FreeRTOS heap is set by the
	configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */
	for( ;; );
}

/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( xTaskHandle pxTask, signed char *pcTaskName )
{
	( void ) pcTaskName;
	( void ) pxTask;

	/* Run time stack overflow checking is performed if
	configconfigCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
	function is called if a stack overflow is detected.  pxCurrentTCB can be
	inspected in the debugger if the task name passed into this function is
	corrupt. */
	for( ;; );
}

/*-----------------------------------------------------------*/

void vApplicationIdleHook( void )
{
	volatile size_t xFreeStackSpace;

	/* The idle task hook is enabled by setting configUSE_IDLE_HOOK to 1 in
	FreeRTOSConfig.h.

	This function is called on each cycle of the idle task.  In this case it
	does nothing useful, other than report the amount of FreeRTOS heap that
	remains unallocated. */
	xFreeStackSpace = xPortGetFreeHeapSize();

	if( xFreeStackSpace > 100 )
	{
		/* By now, the kernel has allocated everything it is going to, so
		if there is a lot of heap remaining unallocated then
		the value of configTOTAL_HEAP_SIZE in FreeRTOSConfig.h can be
		reduced accordingly. */
	}
}

/*-----------------------------------------------------------*/

static void prvSetupHardware( void )
{
	/* TODO: Setup the clocks, etc. here, if they were not configured before
	main() was called. */
}

/*-----------------------------------------------------------*/
/**
	* @brief	Inserts a delay time.
	* @param	t: specifies the delay time length, in uS.
	* @retval	None
	*/
int uS_Delay (int32 t)
{
	int	i=0;
	while (i < t)
		i++;
	return	i;
}


/**
	* @brief	Accumulates Ticks (1 mS each) into Time-Of-Day
	* @param	None
	* @retval	None
	*/
void Do_Time()
{
	uint32 n = TheTime.Ticks;
	portTickType xDelayTime = 10 / portTICK_RATE_MS;

	if (NewSecond)
		return;							//	Do not check Ticks if ISR has not handled previous overflow yet

	if (n & 1)
		Need_Temp  = true;				//	Poll Temperature A/D every odd mS
	else
		Need_Volts = true;				//	Poll Voltage A/D every even mS

	if (n < 1000)
		return;							//	Not a full second yet

	xSemaphoreTake(epoch_Mutex,xDelayTime);	//  lock it
	++TheTime.Epoch;					//	Count another second
	xSemaphoreGive(epoch_Mutex);
	NewSecond = true;					//	Inform ISR of Ticks overflow

	if (n < 2000)						//	If many seconds have built up, only set flag for final second.
		Check_VT = true;				//	Check on Voltage and Temperature averages once per second.

	if ((TheTime.Epoch & 31) == 0)
	{
		Set_Volt_Update_Rate = true;
		Set_Temp_Update_Rate = true;	//	re-establish the A/D conversion periods every 32 seconds
	}
}

uint32 Get_time()
{
	portTickType xDelayTime = 10 / portTICK_RATE_MS;
	uint32 t;
	xSemaphoreTake(epoch_Mutex,xDelayTime);		//  lock it
	t = TheTime.Epoch;							//	grab time
	xSemaphoreGive(epoch_Mutex);
	return t;
}

void Vars_Init()
{
	gPOR = POR_SECONDS;					//	In a state of POR for first X seconds after power-on
	Set_Volt_Update_Rate = true;		//	  until the Vz history is filled in.
	Set_Temp_Update_Rate = true;
	Need_Volts = false;
	Need_Temp = false;
	NewSecond = false;
	Tampered  = false;
	Check_VT  = false;

	Board_Fails = 0;
	Current_Mode = 0x00;
	TheTime.Epoch = 0;					//	Set default time to  1970/1/1
	TheTime.Ticks = 0;

	Config.Periodic_Event_Rate = 300;	//  5 minutes = max time between log points
	Config.Report_Rate = 30*60;			//	30 minutes
}

