/*
 * Copyright (c) 2018 Integrated Solutions, Inc.
 * All rights reserved, worldwide.
 *
 */

#ifndef MODEM_H_INCLUDED
#define MODEM_H_INCLUDED

extern void Modem_Init();
extern void Modem_Test();
extern void modem_PrintRbuf();
extern int modem_SendATCommand(const char *pCmd, int timeout_ms);
extern int modem_ResponseHasOK();


#endif
