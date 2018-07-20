/*
 * Copyright (c) 2018 Integrated Solutions, Inc.
 * All rights reserved, worldwide.
 *
 */

#ifndef UART5_H_INCLUDED
#define UART5_H_INCLUDED

extern void uart5_Init();
extern void uart5_Puts(char const *s);
extern int uart5_PutByte(int ch);
extern void uart5_FlushReceive();

extern int uart5_Write(void const *bufp, int nbytes);

extern int uart5_GetByte(void);	// returns -1 if none available
extern int uart5_WaitForByte(int timeout_ms);	// returns -1 if none available within timeout_ms
extern int uart5_Read(void *bufp, int maxbytes, int timeout_ms);

extern int uart5_Printf(char const *fmt, ... );

#endif
