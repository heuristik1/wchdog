/*
 * micro_vprintf.c
 *
 * This module provides small-footprint replacements for common printf-style
 * formatting routines.
 *
 * These include:
 *
 *  micro_vprintf()         - vprintf() replacement
 *  micro_snprintf()        - snprintf() replacement
 *  micro_vsnprintf()       - vsnprintf() replacement
 *  micro_nprintf()         - doesn't print, just returns length of output
 *
 * Note that only a subset of format specifiers are supported (%u, %d, %x, %c, %s).
 * See micro_vprintf() for details.
 *
 * Copyright (c) 2014 Larry Martin
 * All rights reserved, worldwide.
 *
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "micro_vprintf.h"

/******************************************************************************
 * micro_vprintf()
 *
 * vprintf()-style string formatting with a small memory footprint.
 *
 * Parameters:
 *
 *  outc        output function callback (called as outc(ch,context) for each output char)
 *  context     arbitrary context pointer passed to 'outc' callback
 *  fmt         printf-style format string (supports %u, %x, %d, %s, %c, %d, h, l, width, zero-pad)
 *  valist      variable argument list
 *
 * Returns the number of characters sent to the output callback.
 *
 * Note that this routine does not support all possible printf specifiers.  It does support these:
 *
 *      %u, %d, %x, %c %s %p
 *      'l', 'h' (long, short) modifier
 *      width specifier
 *      zero-pad specifier
 *
 * This routine is intended to support a small-footprint replacement for printf() and friends that
 * handles the most common formatting requirements for debugging output.
 *
 */

int micro_vprintf(	  void (*outc)(int ch, void *context)
					, void *context
					, char const *fmt
					, va_list valist )
{
	int n = 0;          // number of characters written via 'outc'
	static char const *const hexchar = "0123456789ABCDEF";

    while (*fmt)
    {
    	char const *start_fmt = fmt;	        // where we started for this specifier
    	bool isneg = false;
    	bool width_isneg = false;
    	int width = 0;
    	char buffer[24];	                    // big enough for all decimal digits in a 64-bit long
    	int len;
    	uint64_t val;			               // numeric value to convert
    	char modifier = '\0';                   // 'h' or 'l' modifier
    	bool zeropad = false;
    	char *p = &buffer[sizeof(buffer)-1];    // write backwards into the buffer to simplify conversions below
    	*p = '\0';                              // default is an empty string


    	// handle literal characters quickly
        if (*fmt != '%')
        	{
			outc(*fmt++,context);
			n++;
            continue;
        	}

        fmt++;      // skip '%' char

        // Get optional zero-pad flag
        if (*fmt == '0')
        	{
        	zeropad = true;
        	fmt++;
        	}

        // Get optional width specifier
		if (*fmt == '-')		// right-aligned?
			{
			width_isneg = true;
			fmt++;
			}
		while (*fmt >= '0' && *fmt <= '9')
			{
			width = width * 10 + (*fmt - '0');
			fmt++;
			}

		// get optional modifier
		switch (*fmt)
			{
			case 'h':
				modifier = *fmt++;
				break;
			case 'l':
				modifier = *fmt++;
				if (*fmt == 'l')
					{
					modifier = 'L';
					fmt++;
					}
				break;

			}

		// Now handle it;
		switch (*fmt)
			{
			case '\0':	// end of format string (ill-formed), just quit
				return n;

			case '%':   // '%' literal
				*--p = '%';
				break;

			case 'c':   // character
				zeropad = false;
				*--p = (char) va_arg(valist,int);	// promoted to int
				break;

			case 's':   // string
				zeropad = false;
				p = va_arg(valist,char *);	// use string as source buffer
				if (!p)
					p = "(null)";
				break;

			case 'u':   // unsigned
				if (modifier == 'l')
					val = va_arg(valist,unsigned long);
                else if (modifier == 'h')
                    val = (unsigned short) va_arg(valist,unsigned);   // passed in as an unsigned!
                else if (modifier == 'L')
					{
#ifdef __XC32
					// work around bug in XC32 compiler (probably stack alignment problem)
					if (((int) valist & 4) == 0)
						va_arg(valist,unsigned);
#endif
                    // val = (uint64_t) va_arg(valist,uint64_t);
					unsigned lsw = va_arg(valist,unsigned);
					unsigned msw = va_arg(valist,unsigned);
                    val = ((uint64_t) msw << 32) | lsw;					
					}
				else
					val = va_arg(valist,unsigned);
				goto dumpdecimal;

			case 'd':   // decimal
				if (modifier == 'l')
					val = va_arg(valist,long);
				else if (modifier == 'h')
				    val = (short) va_arg(valist,int);   // passed in as an int!
                else if (modifier == 'L')
					{
#ifdef __XC32
					// work around bug in XC32 compiler (probably stack alignment problem)
					if (((int) valist & 4) == 0)
						va_arg(valist,unsigned);
#endif
                    // val = (uint64_t) va_arg(valist,uint64_t);
					unsigned lsw = va_arg(valist,unsigned);
					unsigned msw = va_arg(valist,unsigned);
                    val = ((int64_t) msw << 32) | lsw;
					}
				else
					val = (long) va_arg(valist,int);
				if ((long)val < 0)
					{
					isneg = true;
					val = - (long) val;
					}
dumpdecimal:
                // Convert the number in 'val' to a decimal string
	            do
	            	{
	                *--p = (val % 10) + '0';
	                val /= 10;
	            	} while ( (val != 0) && (p > buffer));
				break;

			case 'p':   // pointer (hex)
				val = (unsigned long) va_arg(valist,void *);
				goto dumphex;

			case 'x':   // hex (lowercase)
			case 'X':   // hex (uppercase)
				if (modifier == 'l')
					val = va_arg(valist,unsigned long);
                else if (modifier == 'h')
                    val = (unsigned short) va_arg(valist,unsigned);   // passed in as an unsigned!
                else if (modifier == 'L')
					{
#ifdef __XC32
					// work around bug in XC32 compiler (probably stack alignment problem)
					if (((int) valist & 4) == 0)
						va_arg(valist,unsigned);
#endif
                    // val = (uint64_t) va_arg(valist,uint64_t);
					unsigned lsw = va_arg(valist,unsigned);
					unsigned msw = va_arg(valist,unsigned);
                    val = ((uint64_t) msw << 32) | lsw;
					}
				else
					val = va_arg(valist,unsigned);
dumphex:
                // Convert the number in 'val' to a hex string
				do
					{
					*--p = hexchar[val % 16];
					if (*fmt == 'x' && *p >= 'A' && *p <= 'F')
						*p += 'a' - 'A';	// convert to lowercase
					val /= 16;
					} while ( (val != 0) && (p > buffer));
				break;

			default:
				// unsupported specifier, just print it as a literal to aid debugging
				while (start_fmt <= fmt)
					{
					outc(*start_fmt++,context);
					n++;
					width--;
					}
			}

		len = strlen(p);

		// Leading pad
		if (!width_isneg)
			{
			if (zeropad && isneg)
				{
				outc('-', context);
				n++;
				width--;
				isneg = false;	// we already handled it
				}

			while (len + (isneg ? 1 : 0) < width)
				{
				outc(zeropad ? '0' : ' ', context);
				n++;
				width--;
				}
			}

		if (isneg)
			{
			outc('-', context);
			n++;
			width--;
			}

		// print the buffer
		while (*p)
			{
			outc(*p++, context);
			n++;
			width--;
			}

		// trailing pad
		while (width > 0)
			{
			outc(' ',context);
			n++;
			width--;
			}

		fmt++;	// skip over the current char in the format string
    	}
	
	return n;
}	

/******************************************************************************
 * micro_nputc()
 *
 * Output callback that does nothing (no actual output)
 *
 * This is used when we want to determine the total length of the output string
 * without actually writing it.
 *
 */

static void micro_nputc(int ch, void *arg)
	{
	}

/******************************************************************************
 * micro_nvprintf()
 *
 * vprintf() analog that doesn't actually print anything, but still returns
 * the number of characters that would have been printed.
 *
 * This can be handy if you need to dynamically allocate space to hold the
 * output string.
 *
 */

int micro_nvprintf(char const *fmt, va_list valist)
	{
	// return the number of characters that would be printed,
	// but without actually printing them
	return micro_vprintf(micro_nputc,NULL,fmt,valist);
	}


/******************************************************************************
 * micro_sputc()
 *
 * Output callback routine to print the formatted output into a string.
 *
 * The 'context' parameter must point to a MicroStrbuf_t structure describing
 * the output buffer and its length.
 *
 */
typedef struct MicroStrbuf_s
{
	char *Buffer;
	int Maxlen;
	int Curlen;
} MicroStrbuf_t;

static void micro_sputc(int ch, void *argp)
{
	MicroStrbuf_t *p = (MicroStrbuf_t *)argp;
	if (p->Curlen < p->Maxlen-1)
	    {
		p->Buffer[p->Curlen++] = ch;
		p->Buffer[p->Curlen] = '\0';	// terminate the string
	    }
	
}

/******************************************************************************
 * micro_vsnprintf()
 *
 * vsnprintf() replacement that uses the micro_vprintf() small-footprint
 * formatter.
 *
 */

int micro_vsnprintf(char *buf, int buflen, char const *fmt, va_list valist)
{
	MicroStrbuf_t b;
	
	*buf = '\0';
	b.Buffer = buf;
	b.Curlen = 0;
	b.Maxlen = buflen;
	
	return micro_vprintf(micro_sputc,&b,fmt,valist);
}


/******************************************************************************
 * micro_snprintf()
 *
 * snprintf() replacement that uses the micro_vprintf() small-footprint
 * formatter.
 *
 */

int micro_snprintf(char *buf, int buflen, char const *fmt, ...)
{
	va_list valist;
	va_start(valist,fmt);
	int n = micro_vsnprintf(buf,buflen,fmt,valist);
	va_end(valist);
	return n;
}

