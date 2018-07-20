/*
 * Copyright (c) 2018 Integrated Solutions, Inc.
 * All rights reserved, worldwide.
 *
 */


/* Scheduler include files */
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include "gsm.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>	// mktime()
#include <ctype.h>  // isspace()

#include "uart5.h"

#if defined(_MSC_VER)
#define DEBUG_PUTS(TXT)			fputs(TXT,stdout)
typedef unsigned int uint32;
uint32 process_ReadEpoch (void);	// until coded
#else
#define DEBUG_PUTS(TXT)			uart1_Puts(TXT)
#include "uart1.h"
#include "armtypes.h"
#include "process.h"
#endif

// -------------------- private constants, data types

#define kERRORS_EMPTY			0
#define kERRORS_NUMERIC			1
#define kERRORS_VERBOSE			2

#define kMAX_BUFFER				(sizeof("\r\nRECEIVE,X,XXXX") + 1460)
#define kMAX_LOGMSG				(128)

// -------------------- private members

// deafault north america
static char g_apn[32] = "ccdy.aer.net";

#if defined(gsm_TEST)
static int g_quit = 0;
#endif

static int default_command_response_timeout_ms = 2000;

struct crt_s {
	char const *at_cmd;
	int s_timeout;
} command_response_timeouts[] = {
	// SimCom manual 
	{ "+CIICR", 85 },
	{ "+CIPSHUT", 65 },
	{ "+CIPSTART", 75 },
	{ "+CIPSEND", 10 },
	{ "+SAPBR", 85 },
    { "+CLBS", 60 },
	{ "+CIPGSMLOC", 60 },
	{ "+CREG", 60 },
	{ NULL, 0 }
};

// -------------------- private methods

static int strnmatch (char const *haystack, char const *needle) {
	int n_match = strlen(needle), is_match = haystack && (strncmp(haystack, needle, n_match) == 0);
	return is_match ? n_match : 0;
}

static char const *strfindend (char const *haystack, char const *needle) {
	char const *f = strstr(haystack, needle);
	return f ? (char const *)(f + strlen(needle)) : NULL;
}

static int get_ms_timeout (char const *cmd) {
	int i;

	if (cmd[0] == 'A' && cmd[1] == 'T') {
		for (i = 0; command_response_timeouts[i].at_cmd; ++i) {
			if (strnmatch(cmd + 2, command_response_timeouts[i].at_cmd) > 0)
				return (command_response_timeouts[i].s_timeout * 1000);
		}
	}
	return default_command_response_timeout_ms;
}

static int matchright (char const *s1, char const *s2) {
	int n1 = strlen(s1), n2 = strlen(s2);

	return (n1 >= n2 && strcmp(&s1[n1 - n2], s2) == 0);
}

static void at_puts (char const *wbuf) {
	int i;

	for (i = 0; wbuf[i]; ++i)
		uart5_PutByte(wbuf[i]);
}

static void scan (char const *str, char const *find, int *pint0, int d0, int *pint1, int d1) {
	char const *p = strstr(str, find);
	int i, n = strlen(find);

	if (pint0)
		*pint0 = d0;
	if (pint1)
		*pint1 = d1;
	if (p) {
		char const *e = p + n;

		i = strtol(e, (char **)&e, 0);
		if (pint0)
			*pint0 = i;
		if (*e == ',' && ++e) {
			i = strtol(e, (char **)&e, 0);
			if (pint1)
				*pint1 = i;
		}
	}
}

static int at_readbytes (char *rbuf, int sz_rbuf, int ms_initial_timeout, int is_AT_command_response) {
	static char const receiveheader[] = "\r\n+RECEIVE,";
	int len_receiveheader = sizeof(receiveheader) - 1;
	int ms_timeout_subsequent = 10;
	int ms_timeout = ms_initial_timeout;
	int is_receive = 0;
	int n_receive = 0;
	//int n_prefix = 0;
	int i;

	rbuf[i = 0] = 0;
	while (i < sz_rbuf - 1) {
		int ch = uart5_WaitForByte(ms_timeout);

		if (ch < 0) {
			if (i == 0)
				return -1;	// got nothing, call it a timeout
			break;
		}

		rbuf[i++] = (char)ch;
		rbuf[i] = 0;
		if (is_AT_command_response) {
			if (matchright(rbuf, "OK\r\n"))
				break;
			if (matchright(rbuf, "ERROR\r\n"))
				break;
			if (matchright(rbuf, "> "))	// CIPSEND
				break;
		} else {
			if (i == len_receiveheader) {
				is_receive = (strcmp(rbuf, receiveheader) == 0);
			} else if (i > len_receiveheader && is_receive && n_receive == 0 && ch == '\n') {
				char *p;

				// parse out n_receive now from something like "\r\n+RECEIVE,2,1348,216.58.219.4:80\r\n"
				p = strchr(rbuf, ',');
				if (p) {
					p = strchr(p + 1, ',');
					if (p) {
						//n_prefix = i;
						n_receive = strtol(p + 1, NULL, 10);
						//logmsg("..receiving %d\n", n_receive);
					}
				}
				if (!p) {
					is_receive = 0;
				}
			}
		}
		// adjust subsequent timeout chars
		if (n_receive)
			ms_timeout = ms_timeout_subsequent * 5;
		else
			ms_timeout = ms_timeout_subsequent;
	}
	return i;
}

static void _at_recv_after_send (char *rbuf, int sz_rbuf, char const *fmt, va_list vl) {
	static char txt[1024];

	vsnprintf(txt, sizeof(txt), fmt, vl);
	txt[sizeof(txt) - 1] = 0;		// MSFT is buggy and won't zero terminate

	// send...
	logbytes("  > ", txt);
	at_puts(txt);			// fputs() doesn't unexpectedly add a newline as puts() does

	if (rbuf) {
		// the read ms_timeout depends on the command sent
		at_readbytes(rbuf, sz_rbuf, get_ms_timeout(txt), 1);
		logbytes("  < ", rbuf);
	}
}

static void at_recv_after_send (char *rbuf, int sz_rbuf, char const *fmt, ...) {
	va_list vl;

	va_start(vl, fmt);
	_at_recv_after_send(rbuf, sz_rbuf, fmt, vl);
	va_end(vl);
}

static void at_send (char const *fmt, ...) {
	va_list vl;

	va_start(vl, fmt);
	_at_recv_after_send(NULL, 0, fmt, vl);
	va_end(vl);
}

void at_sleep() {
	char rbuf[50];
	int sz_rbuf = sizeof(rbuf);

	at_recv_after_send(rbuf, sz_rbuf, "AT+SAPBR=0,1\n");	// close bearer profile first
	at_recv_after_send(rbuf, sz_rbuf, "AT+CFUN=4\n");		// disable tx/rx
	vTaskDelay(MS_TO_TICKS(5000));							// wait 5 seconds
	at_recv_after_send(rbuf, sz_rbuf, "AT+CSCLK=2\n");		// then sleep
}

void at_wake() {
	char rbuf[50];
	int sz_rbuf = sizeof(rbuf);

	at_recv_after_send(rbuf, sz_rbuf, "AT\n");

	at_recv_after_send(rbuf, sz_rbuf, "AT\n");
	at_recv_after_send(rbuf, sz_rbuf, "AT+CSCLK=0\n");
	at_recv_after_send(rbuf, sz_rbuf, "AT+CFUN?\n");		// reset and enable
	at_recv_after_send(rbuf, sz_rbuf, "AT+CFUN=1\n");		// reset and enable
	vTaskDelay(MS_TO_TICKS(2000));							// wait 5 seconds
}


static void at_init (char *rbuf, int sz_rbuf) {
	int int0, int1;

	logmsg("initializing modem\n");
	at_recv_after_send(rbuf, sz_rbuf, "AT\n");
//~	at_recv_after_send(rbuf, sz_rbuf, "ATZ\n");

	at_recv_after_send(rbuf, sz_rbuf, "ATE0\n");	// disable local echo
	at_recv_after_send(rbuf, sz_rbuf, "ATE0\n");	// disable local echo to make sure
	logmsg("getting modem info\n");
	at_recv_after_send(rbuf, sz_rbuf, "AT+CGMM\n");
	at_recv_after_send(rbuf, sz_rbuf, "AT+CGMI\n");
	at_recv_after_send(rbuf, sz_rbuf, "AT+CPIN?\n");

	logmsg("getting sleep mode\n");
	at_recv_after_send(rbuf, sz_rbuf, "AT+CSCLK?\n");
	at_recv_after_send(rbuf, sz_rbuf, "AT+CSCLK=0\n");
	at_recv_after_send(rbuf, sz_rbuf, "AT+CSCLK?\n");

	logmsg("checking registration status\n");
	at_recv_after_send(rbuf, sz_rbuf, "AT+CREG?\n");
	scan(rbuf, "CREG:", &int0, -1, &int1, -1);
	switch (int1) {
		case 1: logmsg("...registered home network\n"); break;
		case 5: logmsg("...registered and roaming\n"); break;
		default: logmsg("...not registered\n"); break;
	}

	logmsg("no sync RTC with network\n");
	at_recv_after_send(rbuf, sz_rbuf, "AT+CLTS=0\n");	// no sync RTC with network time
	logmsg("disable call ready\n");
	at_recv_after_send(rbuf, sz_rbuf, "AT+CIURC=0\n");	// disable call ready URC
	logmsg("getting RSSI\n");
	logmsg("   0 -115 dBm or less\n");
	logmsg("   1 -111 dBm\n");
	logmsg("   2...30 -110... -54 dBm\n");
	logmsg("   31 -52 dBm or greater\n");
	logmsg("   99 not known or not detectable\n");
	at_recv_after_send(rbuf, sz_rbuf, "AT+CSQ\n");		// RSSI

	// buggy:
	//logmsg("getting clock\n");
	//at_recv_after_send(rbuf, sz_rbuf, "AT+CCLK?\n");	// time

	logmsg("set context type, APN, open bearer, query\n");
	at_recv_after_send(rbuf, sz_rbuf, "AT+SAPBR=0,1\n");
	at_recv_after_send(rbuf, sz_rbuf, "AT+SAPBR=3,1,\"Contype\",\"GPRS\"\n");
	at_recv_after_send(rbuf, sz_rbuf, "AT+SAPBR=3,1,\"APN\",\"%s\"\n", g_apn);
	at_recv_after_send(rbuf, sz_rbuf, "AT+SAPBR=1,1\n");
	at_recv_after_send(rbuf, sz_rbuf, "AT+SAPBR=2,1\n");

	logmsg("------ initialized\n");
}

static uint32 scan_time (char const *p) {
	struct tm tm[1];

	memset(tm, 0, sizeof(tm[0]));

	tm->tm_year = strtol(p, (char **)&p, 10) - 1900;
	p += (*p == '/');
	tm->tm_mon = strtol(p, (char **)&p, 10) - 1;
	p += (*p == '/');
	tm->tm_mday = strtol(p, (char **)&p, 10);
	p += (*p == ',');
	tm->tm_hour = strtol(p, (char **)&p, 10);
	p += (*p == ':');
	tm->tm_min = strtol(p, (char **)&p, 10);
	p += (*p == ':');
	tm->tm_sec = strtol(p, (char **)&p, 10);

#if defined(_MSC_VER)
#define mktime _mkgmtime
#endif
	return (uint32)mktime(tm);
}

static char cipgsmloc[128];

static void set_cipgsmloc (char const *p) {
	char *e;

	while (*p == ' ')	// trim leading spaces
		++p;
	strncpy(cipgsmloc, p, sizeof(cipgsmloc));
	cipgsmloc[sizeof(cipgsmloc) - 1] = 0;
	// stop string at control characters
	for (e = cipgsmloc; *e; e++) {
		if (*e < ' ') {	
			*e = 0;
			break;
		}
	}
}

static int scan_gsmloc (char const *str, char const *find, char *long_lat, int sz_long_lat, uint32 *ptime) {
	// looks like "\r\n+CIPGSMLOC: 0,-111.746056,35.198174,2018/03/08,21:02:57\r\n\r\nOK\r\n"
	char const *p = strfindend(str, find), *e;
	int locationcode;

	*long_lat = 0;
	*ptime = 0;
	if (p) {
		// hack since not sure if germany is responding as it should
		set_cipgsmloc(p);
		// resume
		locationcode = strtol(p, (char **)&p, 10);
		if (locationcode == 0) {
			p += (*p == ',');
			e = strchr(p, ',');
			if (e) {
				e = strchr(e + 1, ',');
				if (e) {
					int len = e - p;
					if (len > sz_long_lat - 1)
						len = sz_long_lat - 1;
					strncpy(long_lat, p, len);
					long_lat[len] = 0;
					p = e;
					p += (*e == ',');
					*ptime = scan_time(p);
				}
			}
		}
	}
	return -1;
}

size_t trimwhitespace(char *out, size_t len, const char *str)
{
	if(len == 0)
		return 0;

	const char *end;
	size_t out_size;

	// Trim leading space
	while(isspace((unsigned char)*str))
		str++;

	if(*str == 0)  // All spaces?
		{
		*out = 0;
		return 1;
		}

	// Trim trailing space
	end = str;
	while(!isspace((unsigned char)*end) && ((end-str) < (len-1)))
		end++;

	// Set output size to minimum of trimmed string length and buffer size minus 1
	out_size = (end - str) < len-1 ? (end - str) : len-1;

	// Copy trimmed string and add null terminator
	memcpy(out, str, out_size);
	out[out_size] = 0;

	return out_size;
}

// -------------------- public methods

// get International Mobile Equipment Identity (IMEI)
int gsm_get_IMEI (char *sim_id, int sz) {
	static char rbuf[kMAX_BUFFER];
	static int sz_rbuf = sizeof(rbuf);

    at_init(rbuf, sz_rbuf);

    at_recv_after_send(rbuf, sz_rbuf, "AT+CGSN\n");
    // looks like "\r\n868345036355816\r\n\r\n"
	return trimwhitespace(sim_id, sz, rbuf);
}

// get International Mobile Subscriber Identity (IMSI)
int gsm_get_IMSI (char *sim_id, int sz) {
	static char rbuf[kMAX_BUFFER];
	static int sz_rbuf = sizeof(rbuf);

    at_init(rbuf, sz_rbuf);

    at_recv_after_send(rbuf, sz_rbuf, "AT+CIMI\n");
    // looks like "\r\n868345036355816\r\n\r\n"
	return trimwhitespace(sim_id, sz, rbuf);
}

void gsm_set_apn (char const *apn) {
	strncpy(g_apn, apn, sizeof(g_apn));
	g_apn[sizeof(g_apn) - 1] = 0;
}

int gsm_get_location_and_time (char *long_lat, int sz, uint32 *ptime) {
	static char rbuf[kMAX_BUFFER];
	static int sz_rbuf = sizeof(rbuf);

    at_init(rbuf, sz_rbuf);

    at_recv_after_send(rbuf, sz_rbuf, "AT+CIPGSMLOC=1,1\n");
	return scan_gsmloc(rbuf, "CIPGSMLOC:", long_lat, sz, ptime);
}

const char *gsm_get_cipgsmloc (void) {
	return cipgsmloc;
}

int gsm_send_json (char const *url, char const *auth, char const *json) {
	static char rbuf[kMAX_BUFFER];
	static int sz_rbuf = sizeof(rbuf);
	int i;
	int result=0;	//

	(void)i;
	at_init(rbuf, sz_rbuf);

	logmsg("enable HTTP\n");
	at_recv_after_send(rbuf, sz_rbuf, "AT+HTTPTERM\n");
	at_recv_after_send(rbuf, sz_rbuf, "AT+HTTPINIT\n");
	at_recv_after_send(rbuf, sz_rbuf, "AT+HTTPSSL=1\n");

	logmsg("transmit\n");
	at_recv_after_send(rbuf, sz_rbuf, "AT+HTTPPARA=\"CID\",1\n");
	at_recv_after_send(rbuf, sz_rbuf, "AT+HTTPPARA=\"URL\",\"%s\"\n", url);
	at_recv_after_send(rbuf, sz_rbuf, "AT+HTTPPARA=\"USERDATA\",\"Authorization: %s\"\n", auth);
	at_recv_after_send(rbuf, sz_rbuf, "AT+HTTPPARA=\"CONTENT\",\"application/json\"\n");

	at_recv_after_send(rbuf, sz_rbuf, "AT+HTTPDATA=%d,20000\r\n", strlen(json));
	at_send("%s", json);
	i = at_readbytes(rbuf, sz_rbuf, 20000, 0);
	logbytes("  = ", rbuf);

	at_recv_after_send(rbuf, sz_rbuf, "AT+HTTPACTION=1\r\n");
	// wait for the \r\n+HTTPACTION: 1, 400,157\r\n
	i = at_readbytes(rbuf, sz_rbuf, 20000, 0);
	logbytes("  = ", rbuf);
	if (strstr(rbuf,"204") != NULL)
		result = 1;

	at_recv_after_send(rbuf, sz_rbuf, "AT+HTTPREAD\r\n");
	i = at_readbytes(rbuf, sz_rbuf, 20000, 0);
	logbytes("  = ", rbuf);

//	at_recv_after_send(rbuf, sz_rbuf, "AT+CEER\r\n");
	at_recv_after_send(rbuf, sz_rbuf, "AT+HTTPTERM\n");
	return result;
}

#if 0

static int find_in_expect_list (char const *rbuf, va_list vl) {
	int idx;
	char const *str;

	for (idx = 0; (str = va_arg(vl, char const *)) != NULL; ++idx) {
		if (matchright(rbuf, str))
			return idx;
	}
	return -1;
}

static int at_wait_expect (char *rbuf, int sz_rbuf, int ms_full_timeout, ...) {
	int ms_wait = 100;
	int idx = -1;
	va_list vl;

	logbytes("  ~ ", "");
	while (!g_quit && ms_full_timeout > 0 && idx == -1) {
		at_readbytes(rbuf, sz_rbuf, ms_wait, 1);
		va_start(vl, ms_full_timeout);
		idx = find_in_expect_list(rbuf, vl);
		va_end(vl);
		ms_full_timeout -= ms_wait;
	}
	logbytes("  < ", rbuf);
	return idx;
}

void get_tcp (void) {
	char const *host = "www.google.com";
	char const *page = "/";
	int port = 80;
	char rbuf[kMAX_BUFFER];
	int sz_rbuf = sizeof(rbuf);
	char wbuf[256];
	int sz_wbuf = sizeof(wbuf);
	int isock = 2;
	int i;

	at_init(rbuf, sz_rbuf);

	logmsg("shutdown any previous\n");
	at_recv_after_send(rbuf, sz_rbuf, "AT+CIPSHUT\n");

	logmsg("set reporting level of errors\n");
	at_recv_after_send(rbuf, sz_rbuf, "AT+CMEE=%d\n",kERRORS_VERBOSE);
	at_recv_after_send(rbuf, sz_rbuf, "AT+CMEE=%d\n",kERRORS_EMPTY);

	logmsg("get service info\n");
	at_recv_after_send(rbuf, sz_rbuf, "AT+CGATT?\n");

	logmsg("multiple connections mode, less buggy\n");
	at_recv_after_send(rbuf, sz_rbuf, "AT+CIPMUX=1\n");

	logmsg("set APN\n");
	at_recv_after_send(rbuf, sz_rbuf, "AT+CSTT=\"%s\",\"\",\"\"\n", g_apn);

	logmsg("query gprs\n");
	at_recv_after_send(rbuf, sz_rbuf, "AT+CIICR=?\n");

	logmsg("startup gprs\n");
	at_recv_after_send(rbuf, sz_rbuf, "AT+CIICR\n");

	logmsg("get local IP addr\n");
	at_recv_after_send(rbuf, sz_rbuf, "AT+CIFSR\n");

	logmsg("------ ready\n");

	logmsg("query ip addr\n");
	at_recv_after_send(rbuf, sz_rbuf, "AT+CDNSGIP=\"%s\"\n", host);

	logmsg("disable quick send mode\n");
	at_recv_after_send(rbuf, sz_rbuf, "AT+CIPQSEND=0\n");
	
	logmsg("set +RECEIVE,n,length\n");
	at_recv_after_send(rbuf, sz_rbuf, "AT+CIPSRIP=1\n");

	logmsg("connect\n");
	at_recv_after_send(rbuf, sz_rbuf, "AT+CIPSTART=%d,\"TCP\",\"%s\",%d\n", isock, host, port);
	at_wait_expect(rbuf, sz_rbuf, 10000, "OK\r\n", "FAIL\r\n", NULL);

	logmsg("--- data\n");

	logmsg("sending data\n");
	snprintf(wbuf, sz_wbuf, ""
			"GET %s HTTP/1.1\n"
			"Host: %s\n"
			"\n"
			, page, host);
	at_recv_after_send(rbuf, sz_rbuf, "AT+CIPSEND=%d,%d\n", isock, strlen(wbuf));
	at_send("%s", wbuf);
	at_wait_expect(rbuf, sz_rbuf, 10000, "OK\r\n", "FAIL\r\n", NULL);

	i = at_readbytes(rbuf, sz_rbuf, 10000, 0);
	logbytes("  = ", rbuf);

	logmsg("------ close / shutdown\n");

	logmsg("shutting down connection\n");
	at_recv_after_send(rbuf, sz_rbuf, "AT+CIPCLOSE=%d\n", isock);

	at_recv_after_send(rbuf, sz_rbuf, "AT+CIPSHUT\n");
}
#endif

void logmsg (char const *fmt, ...) {
	char txt[kMAX_LOGMSG];
	va_list vl;

	va_start(vl, fmt);
	vsnprintf(txt, sizeof(txt), fmt, vl);
	va_end(vl);
	txt[sizeof(txt) - 1] = 0;		// MSFT is buggy and won't zero terminate

	DEBUG_PUTS(txt);
}

void logbytes (char const *prefix, void const *bytes) {
	char const *str = (char const *)bytes;
	int i;

	if (prefix)
		DEBUG_PUTS(prefix);
	for (i = 0; str[i]; i++) {
		unsigned char ch = (unsigned char)str[i];
		switch (ch) {
			case '\\': DEBUG_PUTS("\\\\"); break;
			case '\n': DEBUG_PUTS("\\n"); break;
			case '\r': DEBUG_PUTS("\\r"); break;
			case '\t': DEBUG_PUTS("\\t"); break;
			case '"': DEBUG_PUTS("\\\""); break;
			case '\'': DEBUG_PUTS("\\'"); break;
			default:
				if (ch < ' ') {
					char hex[8];
					sprintf(hex, "\\x%02x", ch);
					DEBUG_PUTS(hex);
				} else {
					char chstr[2];
					// everything else, including UTF-8 multibyte, put down literally
					chstr[0] = ch;
					chstr[1] = 0;
					DEBUG_PUTS(chstr);
				}
				break;
		}
	}
	DEBUG_PUTS("\n");
}

// ============================================================================
#if defined(gsm_TEST)

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
//#include "debug_expect_inl.h"
//#include "debug_leakcheck_inl.h"	// put at top and brace with defined(..._TEST)

#include "serialportx.c"
#include "clockx.c"

// -------- uart5

static serialport_tt *pPort;
static char com_port_name[128] = "COM11";
static char com_open_name[128];

int uart5_WaitForByte (int ms_timeout) {
	if (pPort) {
		double t0 = monotonic(), delta = ms_timeout * 0.001, d;

		while (!g_quit && (d = difftimex(monotonic(), t0)) < delta) {
			int ch = readbyte_serial(pPort);

			//logmsg("%.3lf %.3lf 0x%02x %c\n", monotonic(), d, ch, ch >= ' ' && ch < 0x7f ? ch : '?');
			if (ch >= 0)
				return ch;
		}
	}
	return -1;	// nothing ready
}

int uart5_PutByte (int ch) {
	if (pPort) {
		unsigned char c = (unsigned char)ch;

		write_serial(pPort, &c, 1);
		return 1;
	}
	return 0;
}

void ms_sleep (int ms) {
#if defined(_MSC_VER)
	dsleep(0.001 * ms);
#else
	portTickType xDelayTime = ms / portTICK_RATE_MS;
	vTaskDelay(xDelayTime);
#endif
}

uint32 process_ReadEpoch (void) {
	return (uint32)time(NULL);
}

// ------

static void on_signal (int i) {
	if (i == SIGINT && !g_quit)
		fprintf(stdout, "\n^C\n");
	g_quit |= (i == SIGINT);
	signal(i, on_signal);
}

static void test (char const *s, char const *f, int exp0, int exp1) {
	int int0, int1;
	scan(s, f, &int0, -1, &int1, -1);
	printf("...scanned %d and %d\n expecting %d and %d\n", int0, int1, exp0, exp1);
}

static void send_iot (void) {
	static char const url[] = "https://watchdog-hub.azure-devices.net/devices/beta1/messages/events?api-version=2016-02-03";
	static char const auth[] = "SharedAccessSignature sr=watchdog-hub.azure-devices.net%2Fdevices%2Fbeta1&sig=sLzFxl4Pj3j2MCuqUa5c8jk25oAa0UWWTr7A6vT7c5E%3D&se=1552065965";
	static char const json[] = "{\"id\":\"000000\",\"v\":24.0;\"tc\":20.0}";
	gsm_send_json(url, auth, json);
}

int main(int argc, char const *argv[]) {
	if (argc) argv;

	if (0,0) {
		test("\r\n+CSQ: 11,0\r\n\r\nOK\r\n", "CSQ:", 11, 0);
		test("\r\n+CSQ: 11\r\n\r\nOK\r\n", "CSQ:", 11, 0);
		test("\r\n+CSQ: 0,11\r\n\r\nOK\r\n", "CSQ:", 0, 11);
		return 0;
	}
	if (0,0) {
		if (matchright("\r\n2, CONNECT OK\r\n", "OK\r\n"))
			printf("match\n");
		else
			printf("no match\n");
		return 0;
	}
	if (1,1) {
		char long_lat[128];
		uint32 t;
		scan_gsmloc("\r\n+CIPGSMLOC: 0,-111.746056,35.198174,2018/03/08,21:02:57\r\n\r\nOK\r\n",
			"CIPGSMLOC:", long_lat, sizeof(long_lat), &t);
		printf("long_lat: '%s'\n", long_lat);
		printf("epoch: %d\n", t);
	}

	signal(SIGINT, on_signal);

	if (argc > 1) {
		strcpy(com_port_name, argv[1]);
	}

	sprintf(com_open_name, "%s:115200", com_port_name);
	pPort = open_serial(com_open_name);
	if (pPort)
		logmsg("connected: %s\n", com_open_name);
	else
		logmsg("not found: %s\n", com_open_name);

	send_iot();

	return 0;
}

#endif	/* #if defined(gsm_TEST) */



