/*
 * Copyright (c) 2018 Integrated Solutions, Inc.
 * All rights reserved, worldwide.
 *
 */


#if !defined(gsm_h_INCLUDED_HEADER)
#define      gsm_h_INCLUDED_HEADER

#include "armtypes.h"

// --------------------
#if defined(__cplusplus)
extern "C" {
#endif
	
// -------------------- public constants, data types

// -------------------- public methods

extern void at_sleep();
extern int at_wake();
extern void do_init();
extern void ss_at_wake();

void gsm_set_apn (char const *apn);
int gsm_get_IMEI (char *sim_id, int sz);
int gsm_get_IMSI (char *sim_id, int sz);
void gsm_get_location_and_time (char *long_lat, int sz, uint32 *ptime);
const char *gsm_get_cipgsmloc (void);

int gsm_send_json (char const *url, char const *auth, char const *json);

void logmsg (char const *fmt, ...);
void logbytes (char const *prefix, void const *bytes);

#if defined(__cplusplus)
}
#endif

#endif	/* ..._h_INCLUDED_HEADER... */

