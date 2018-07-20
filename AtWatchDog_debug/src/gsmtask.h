/*
 * Copyright (c) 2018 Integrated Solutions, Inc.
 * All rights reserved, worldwide.
 *
 */

#ifndef GSMTASK_H_INCLUDED
#define GSMTASK_H_INCLUDED

#include "main.h"

extern void prvCellTask( void *pvParameters );
extern bool gHoldSamplesDuringModemActivity;

#endif
