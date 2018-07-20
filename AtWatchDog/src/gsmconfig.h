/*
 * Copyright (c) 2018 Integrated Solutions, Inc.
 * All rights reserved, worldwide.
 *
 */





#define USING_ISITEST01




// ---------- configurations

#if defined(USING_BETA1)
//#define TOMS_PCB_NUMBER	"1"
//#define IMEI_NUMBER		"868345036362960"
#define SAS_NAME			"beta1"
static char const g_auth[] = "SharedAccessSignature sr=watchdog-hub.azure-devices.net%2Fdevices%2Fbeta1&sig=sLzFxl4Pj3j2MCuqUa5c8jk25oAa0UWWTr7A6vT7c5E%3D&se=1552065965";
static char const g_apn[] = "ccdy.aer.net";
#endif

#if defined(USING_BETA2)
//#define TOMS_PCB_NUMBER	"5"
//#define IMEI_NUMBER		"868345036358950"
#define SAS_NAME			"beta2"
static char const g_auth[] = "SharedAccessSignature sr=watchdog-hub.azure-devices.net%2Fdevices%2Fbeta2&sig=kQzwN0TFuLKUANaWbGkk1H37uv0oWesGsjY3tUgkDyg%3D&se=1552149402";
static char const g_apn[] = "ccdy.aer.net";
#endif

#if defined(USING_BETA7)
//#define TOMS_PCB_NUMBER	"7"
//#define IMEI_NUMBER		"868345036372688"
#define SAS_NAME			"beta7"
static char const g_auth[] = "SharedAccessSignature sr=watchdog-hub.azure-devices.net%2Fdevices%2Fbeta7&sig=f%2FLaPstDVN%2FUAZb9emyipjR8MEgcih7ZwyzLqPxwp3k%3D&se=1552154272";
static char const g_apn[] = "ccdy.aer.net";
#endif

#if defined(USING_BETA9)
//#define TOMS_PCB_NUMBER	"9"
//#define IMEI_NUMBER		"868345036355550"
#define SAS_NAME			"beta9"
static char const g_auth[] = "SharedAccessSignature sr=watchdog-hub.azure-devices.net%2Fdevices%2Fbeta9&sig=QxbA%2F%2F4FMFf0GgUnfjYuwXxIZzRumYDLpRJnSUSdeBc%3D&se=1552154446";
static char const g_apn[] = "ccdy.aer.net";
#endif

// ---

#if defined(USING_AKKU1)
#define SAS_NAME			"AKKU1"
static char const g_auth[] = "SharedAccessSignature sr=watchdog-hub.azure-devices.net%2Fdevices%2FAKKU1&sig=gCNJ2E7i3gEIaqwVNUKQNhJB1iLdYyWCmLS37nZZ1ZQ%3D&se=1552187479";
static char const g_apn[] = "ccdy-eu.aer.net";	// Germany in EU
#endif

#if defined(USING_AKKU2)
#define SAS_NAME			"AKKU2"
static char const g_auth[] = "SharedAccessSignature sr=watchdog-hub.azure-devices.net%2Fdevices%2FAKKU2&sig=PAphoqjAK5eSIzgvYZZoV2pAyRQ%2BxCOe%2FqBqpiofS3k%3D&se=1552187512";
static char const g_apn[] = "ccdy-eu.aer.net";	// Germany in EU
#endif

// --- WD series

#if defined(USING_WD00010)
#define SAS_NAME			"WD00010"
static char const g_auth[] = "SharedAccessSignature sr=watchdog-hub.azure-devices.net%2Fdevices%2FWD00010&sig=BIKPvNC1D8%2BofdT0a9IOjTBrQzR%2FHozj7F7leKmb5eI%3D&se=1557542678";
static char const g_apn[] = "ccdy-eu.aer.net";	// Germany in EU
#endif

#if defined(USING_WD00011)
#define SAS_NAME			"WD00011"
static char const g_auth[] = "SharedAccessSignature sr=watchdog-hub.azure-devices.net%2Fdevices%2FWD00011&sig=uIuTjB47JgLA9CcvmUYZBbNL%2FdXHbjhmj5mcXYm1aH8%3D&se=1557542666";
static char const g_apn[] = "ccdy-eu.aer.net";	// Germany in EU
#endif

#if defined(USING_WD00012)
#define SAS_NAME			"WD00012"
static char const g_auth[] = "SharedAccessSignature sr=watchdog-hub.azure-devices.net%2Fdevices%2FWD00012&sig=DrhfQhIZn4pwClDyQ3hg%2FBMVVLpK5gCHsQOB2FqgWnY%3D&se=1557542654";
static char const g_apn[] = "ccdy-eu.aer.net";	// Germany in EU
#endif

#if defined(USING_WD00013)
#define SAS_NAME			"WD00013"
static char const g_auth[] = "SharedAccessSignature sr=watchdog-hub.azure-devices.net%2Fdevices%2FWD00013&sig=mvqx%2FJ1MbCzpI34D3FLNIvR0B7UisLGgIEx%2BrmB%2BxOI%3D&se=1557542610";
static char const g_apn[] = "ccdy-eu.aer.net";	// Germany in EU
#endif

#if defined(USING_WD00014)
#define SAS_NAME			"WD00014"
static char const g_auth[] = "SharedAccessSignature sr=watchdog-hub.azure-devices.net%2Fdevices%2FWD00014&sig=StdeCqDm1JHLmvnjaN7Gi%2FVpekipnOGd7UW6c2qfVKs%3D&se=1557542641";
static char const g_apn[] = "ccdy-eu.aer.net";	// Germany in EU
#endif

// --- 

#if defined(USING_WD00015)
#define SAS_NAME			"WD00015"
static char const g_auth[] = "SharedAccessSignature sr=watchdog-hub.azure-devices.net%2Fdevices%2FWD00015&sig=kjPiYAMi6Y4D3shBxBERjXF2si1FchmyP%2Fy3AcvKNus%3D&se=1558126010";
static char const g_apn[] = "ccdy-eu.aer.net";	// Germany in EU
#endif

#if defined(USING_WD00016)
#define SAS_NAME			"WD00016"
static char const g_auth[] = "SharedAccessSignature sr=watchdog-hub.azure-devices.net%2Fdevices%2FWD00016&sig=MdT%2B%2F%2BYcCKmx%2B32Y4Th7wXIeuRX0f2SsJl%2BJgjN9KbU%3D&se=1558125812";
static char const g_apn[] = "ccdy-eu.aer.net";	// Germany in EU
#endif

#if defined(USING_WD00017)
#define SAS_NAME			"WD00017"
static char const g_auth[] = "SharedAccessSignature sr=watchdog-hub.azure-devices.net%2Fdevices%2FWD00017&sig=uRr4%2FjcQSVB7scuMn5lqy4cr5I4gJdwxlAIMxH%2Bnnms%3D&se=1558126041";
static char const g_apn[] = "ccdy-eu.aer.net";	// Germany in EU
#endif

#if defined(USING_WD00018)
#define SAS_NAME			"WD00018"
static char const g_auth[] = "SharedAccessSignature sr=watchdog-hub.azure-devices.net%2Fdevices%2FWD00018&sig=CpRXOM0Oc6ytMYxpV3X%2BOebKow4VuUmJcHKFa49P%2BTI%3D&se=1558126066";
static char const g_apn[] = "ccdy-eu.aer.net";	// Germany in EU
#endif

#if defined(USING_WD00019)
#define SAS_NAME			"WD00019"
static char const g_auth[] = "SharedAccessSignature sr=watchdog-hub.azure-devices.net%2Fdevices%2FWD00019&sig=wXh38Tx8mWHs2AuOrNLE2%2FGDeQDEwdfb6p%2B9%2BdLkshE%3D&se=1558126083";
static char const g_apn[] = "ccdy-eu.aer.net";	// Germany in EU
#endif

#if defined(USING_WD00020)
#define SAS_NAME			"WD00020"
static char const g_auth[] = "SharedAccessSignature sr=watchdog-hub.azure-devices.net%2Fdevices%2FWD00020&sig=ky5kYNnqpNkJpIqFq0%2B0%2FjI8PyJlstFP8d7n4miZWZQ%3D&se=1558126099";
static char const g_apn[] = "ccdy-eu.aer.net";	// Germany in EU
#endif

// ---

#if defined(USING_ISITEST01)
//#define TOMS_PCB_NUMBER	"2"
//#define IMEI_NUMBER		"868345036355816"
#define SAS_NAME			"isitest01"
static char const g_auth[] = "SharedAccessSignature sr=watchdog-hub.azure-devices.net%2Fdevices%2Fisitest01&sig=CylRnJBXbsAFM%2B328u9NhXcm1jO2ovZ9DvQJSxIxJvQ%3D&se=1552256387";
static char const g_apn[] = "ccdy.aer.net";
#endif

#if defined(USING_ISITEST02)
//#define TOMS_PCB_NUMBER	"4"
//#define IMEI_NUMBER		"868345036358802"
#define SAS_NAME			"isitest02"
static char const g_auth[] = "SharedAccessSignature sr=watchdog-hub.azure-devices.net%2Fdevices%2Fisitest02&sig=hIg%2FaXKZRXNBrH4viIHyyrKoJVZqxn6Y192v2B%2FCu%2Fk%3D&se=1552256570";
static char const g_apn[] = "ccdy.aer.net";
#endif

// ----------- derived from above

static char const g_name[] = SAS_NAME;
static char const g_url[] = "https://watchdog-hub.azure-devices.net/devices/" SAS_NAME "/messages/events?api-version=2016-02-03";

