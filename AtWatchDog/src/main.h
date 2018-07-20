/*
 * Copyright (c) 2018 Integrated Solutions, Inc.
 * All rights reserved, worldwide.
 *
 */

/**
  ******************************************************************************
  * @file    main.h
  * @author  MCD Application Team
  * @version V1.2.0
  * @date    11-April-2014
  * @brief   Header for main.c module
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2014 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>

/* Exported types ------------------------------------------------------------*/
typedef uint8_t		bool;
typedef uint8_t		uint8;
typedef uint16_t	uint16;
typedef uint32_t	uint32;
typedef int32_t		int32;

typedef struct
{
	uint32	Epoch;		//	seconds since 1970
	uint32	Ticks;		//	milliseconds seen by System Tick
} TheTime_t;

/* Exported variables --------------------------------------------------------*/
extern bool	Need_Volts;
extern bool	Need_Temp;
extern bool	NewSecond;
extern bool	Tampered;
extern bool	Check_VT;
extern bool Set_Volt_Update_Rate;
extern bool Set_Temp_Update_Rate;
extern int  Battery_Size;			//	1,2,3,4 = number of 12v packs in the battery
extern int  Current_Mode;			//	0,1,2   = current battery mode
extern int	gPOR;
extern int	Periodic;
extern int	Board_Fails;

extern TheTime_t	TheTime;


/* Exported constants --------------------------------------------------------*/

#define  POR_SECONDS	30
#define  false			0
#define  true			1
#define  TEMP_FAIL		1
#define  VOLT_FAIL		2
#define  FLASH_FAIL		4
#define  DEBUG_FAIL		8
#define  CELL_FAIL		16


/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
extern int  uS_Delay (int32 t);
extern void Vars_Init();
extern uint32 Get_time();

#define MS_TO_TICKS(ms)	((ms) / portTICK_RATE_MS)
#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
