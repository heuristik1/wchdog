/*
 * Copyright (c) 2018 Integrated Solutions, Inc.
 * All rights reserved, worldwide.
 *
 */

#ifndef UART1_H_INCLUDED
#define UART1_H_INCLUDED

extern void uart1_Init();
extern void uart1_Puts(char const *s);
extern int uart1_PutByte(int ch);
extern void uart1_PutLine(char const *s);


extern int uart1_Write(void const *bufp, int nbytes);

extern int uart1_GetByte(void);	// returns -1 if none available
extern int uart1_WaitForByte(int timeout_ms);	// returns -1 if none available within timeout_ms
extern int uart1_Read(void *bufp, int maxbytes, int timeout_ms);

extern int uart1_Printf(char const *fmt, ... );

#endif
