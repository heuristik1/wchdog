/*
 * Copyright (c) 2018 Integrated Solutions, Inc.
 * All rights reserved, worldwide.
 *
 */

#ifndef VERSION_INCLUDED
#define VERSION_INCLUDED

#define VERSION	"0.014"

/*

Rev.	Description
-----	------------------------------------------------------------------

0.014 - 2018May17 __pla copyrights

0.013 - 2018May15 -tkr, Added IMSI instead of IMEI for JSON payload

0.012 - 2018May11 __pla, Roger and I think the header wasn't updated for the
		Successful_Xfer_Callback() fix, but we're not sure

0.011	5-2-2018 -tkr
		linear tech regulator and some minor changes

0.010	5-2-2018 -tkr
		linear tech regulator and some minor changes
		forced tx and rx off so the modem is quiet
		TODO: need to not log as sent if POST is rejected

0.009	4-24-2018 -tkr
		adding temp delta logging

0.008	4-17-2018 -tkr
		fixed to what the really want

0.007	4-11-2018 -tkr/rj
		coding to spec for mode change
		<cough> forward looking
		getting IMEI from SIM card
		30 minute intervals

0.006	3-15-2018 -tkr
		fixed 24v input limit

0.005	3-10-2018 -tkr
		LED changes

0.004	3-8-2018 -tkr
		POST works with SaS token
		find_location() added

0.003	3-7-2018 -tkr
		fixed nasty bug in uart semaphore timeout
		integration of gsm code

0.002	3-6-2018 -tkr
		Roger fixes for flash
		Start up modem code

0.001	3-5-2018 -tkr
		Initial version to start from (shell)

 */


#endif
