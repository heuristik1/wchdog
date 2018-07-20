/*
 * Copyright (c) 2018 Integrated Solutions, Inc.
 * All rights reserved, worldwide.
 *
 */

#ifndef DEBUG_H_INCLUDED
#define DEBUG_H_INCLUDED

#include "uart1.h"

extern void debug_Init();
extern void debug_Puts(const char *s);
extern void debug_PutLine (char const *s);
extern void debug_Putc(const char c);

#define debug_Printf uart1_Printf

#endif
