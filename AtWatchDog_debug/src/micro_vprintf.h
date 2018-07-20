/*
 * Copyright (c) 2014 Larry Martin
 * All rights reserved, worldwide.
 *
 */

#ifndef micro_vprintf_h_included
#define micro_vprintf_h_included

#include <stdarg.h>
	
#ifdef __cplusplus
extern "C" {
#endif
	
	
// Generic output callback function	
extern int micro_vprintf( void (*outc)(int ch, void *argp), void *argp, char const *fmt, va_list valist);

// Convenient replacements for common usages
extern int micro_vsnprintf(char *buf, int buflen, char const *fmt, va_list valist);
extern int micro_snprintf(char *buf, int buflen, char const *fmt, ...);
extern int micro_nvprintf(char const *fmt, va_list valist);	// returns number of chars that would be written


#ifdef __cplusplus
}
#endif


#endif
