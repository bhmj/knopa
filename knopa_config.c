
#include <user_interface.h>
#include <eagle_soc.h>

//#include <osapi.h>
#include <gpio.h>

#include "knopa_config.h"
#include "knopa_defaults.h"
#include "knopa_core.h"
#include "knopa_wifi.h"
#include "knopa_dbg.h"
#include "knopa_fs.h"
#include "knopa_misc.h"
#include "knopa_string.h"
#include "knopa_json.h"

#define FS_SECTOR_START 0x4F

#define MAX_CONDITIONS	4
#define MAX_ACTIONS		4

#define SUPPORTED_FILE "supported.json"
#define INSTALLED_FILE "installed.json"
#define RULES_FILE     "rules.json"
#define RULES_FILE_BIN "rules.bin"

// ============================================================================

t_KnopaEnv knopaEnv;

t_KnopaConfig knopaConfig;

typedef struct {
	uint16 cid;
	uint16 parsed; // 0, 1, 'P'=period, 'C'=cron, '1'=pair of values, '2'=two pairs of values
	union {
		char txt[48]; // asciizz
		struct {
			uint8 d[2], h[2], m[2], s[2]; // period from..to
			uint8 mi, hh, dd, mm, dw; // cron
			uint16 yy; // cron
			int ival[4];     // pair of values
		} values;	
	};
} t_Condition; // = 52 bytes

typedef struct {
	uint16 aid;
	uint16 parsed; // 0, 1, 2=int, 3=text, 4=int+text, 5=text+int
	char txt[48];
} t_Action; // = 52 bytes

typedef struct {
	char code[16]; // 8 cyrillic in UTF-8
	char name[64]; // 32 cyrillic in UTF-8
	uint16 nConditions;
	uint16 nActions;
	t_Condition conditions[MAX_CONDITIONS];
	t_Action actions[MAX_ACTIONS];
} t_Rule; // =

#define MAX_RULES 8
uint16 nRules;
t_Rule rules[MAX_RULES];

typedef struct {
	uint16 type; // int, decimal, string
	int32 ivalue;
} t_ModuleParam;

typedef struct {
	uint16 hwid;
	uint16 type;
	char sproto[16];
	char sparams[128];
	uint16 proto;
	t_ModuleParam params[4];
	uint16 nunits;
} t_Supported;

t_Supported supported[32];

// ============================================================================
// Forward declarations
//
static void FLASH_CODE Debug_HardwareConfig (void);
static void FLASH_CODE SaveConfig (void);
static bool FLASH_CODE SaveRules (char* p);
static void FLASH_CODE InitDone_cb (void);
static bool FLASH_CODE ProcessRequest (enRequestMethod method, char* path, char* params, int paramCount, t_HttpOut* httpout);

// ============================================================================
// Knopa runtime entry
//
//  In:  config_gpio      : default config button (could be overwritten)
//
//  Out: none
//
void  FLASH_CODE KnopaInit (uint8 config_gpio)
{
	gpio_init();
	uart_div_modify(0, UART_CLK_FREQ / 115200);

	os_printf("Knopa v.0.3a\n");
	os_printf("Heap: %d\n", system_get_free_heap_size());

	// configure "config" button
	gpio_output_set(0, 0, 0, 1<<config_gpio);
	PIN_PULLUP_DIS(PERIPHS_IO_MUX_GPIO5_U);

	// check "config" button state
	knopaEnv.isConfigMode = true; // GPIO_INPUT_GET(config_gpio);

	// WiFi
	wifi_station_set_auto_connect(FALSE); // do not autoconnect yet

	if (knopaEnv.isConfigMode) {
		// Run RTC timer
		RunRTCTimer(); // 0.1 sec
		SetBlinky(0, 10, 5); // pin, cycle, duty (i.e. 1 sec cycle, 0.5 sec duty)

		// Print some general interest info
		struct rst_info *rtc_info = system_get_rst_info();
		char* rst_reason_txt[] = { "normal powerup", "HW watchdog", "exception", "SW watchdog", "SW restart", "deep-sleep awake", "external" };
		dbg_printf(2,"\nReset reason: %s (%x)\n", (rtc_info->reason>6 ? "unknown" : rst_reason_txt[rtc_info->reason]), rtc_info->reason);
	}

	// Postpone
	system_init_done_cb(InitDone_cb);
}

// ============================================================================
// Scan APs callback
//
//  In:  pespconn         : connection handle
//       success          : TRUE/FALSE (general result)
//       bss_info* aps    : AP data (single-linked list)
//
//  Out: none
//
static void FLASH_CODE InitDone_cb (void)
{
	char* offon[] = {"OFF","ON"};
	sint16 i;
	char flags[4];

	Debug_HardwareConfig();

	dbg_printf(1, "\nSystem init done: %s, config mode = %s\n", knopaConfig.magic, offon[knopaEnv.isConfigMode]);

	uint32 fs_len = user_rf_cal_sector_set() - FS_SECTOR_START;
	if (!KFS_Init(FS_SECTOR_START, fs_len)) {
		if (!KFS_Format(FS_SECTOR_START, fs_len)) {
			dbg_printf(0, "KFS error: %s\n", KFS_ErrorMessage());
		} else {
			dbg_printf(0, "Format OK\n");
		}
	} else {
		dbg_printf(0, "KFS Init OK\n");
	}

	t_File* f;
	bool savedef = false;
	os_memset(&knopaConfig, sizeof(knopaConfig), 0);
	f = KFS_Open("knopa.config", "r", "", 0);
	if (!f) { os_printf("saving default settings\n"); savedef = true; }
	else if (!KFS_Read(f, (char*)&knopaConfig, sizeof(knopaConfig))) savedef = true;
	KFS_Close(f);
	if (savedef)
	{
		// load defaults
		os_strcpy(knopaConfig.magic, def_magic);
		os_strcpy(knopaConfig.apssid, def_apssid);
		os_strcpy(knopaConfig.appass, def_appass);
		os_strcpy(knopaConfig.name, def_name);
		os_strcpy(knopaConfig.ntp_servers, def_ntp_servers);
		os_strcpy(knopaConfig.wifissid, def_wifissid);
		os_strcpy(knopaConfig.wifipass, def_wifipass);
		*knopaConfig.wifibssid = 0;
		os_strcpy(knopaConfig.cloud, def_cloud);
		os_strcpy(knopaConfig.hardware, def_hardware);
		knopaConfig.measure = 600;
		knopaConfig.powersave = 1;
		knopaConfig.autoupd = 0;
		knopaConfig.dbglvl = 0;
		knopaConfig.posix_time = 0;
		knopaConfig.timezone = 180;
		knopaConfig.clconn = 0;
		knopaConfig.clsync = 0;
		knopaConfig.hiddenAP = 0;
		// save
		f = KFS_Open("knopa.config", "w", "application/x-knopa-config", sizeof(knopaConfig));
		if (!f) { os_printf("File error (1): %s\n", KFS_ErrorMessage()); }
		else {
			os_printf("writing...\n");
			if (!KFS_Write(f, (char*)&knopaConfig, sizeof(knopaConfig))) { os_printf("File error (2): %s\n", KFS_ErrorMessage()); };
		}
		KFS_Close(f);
		os_printf("Default config saved\n");
	}
	
	if (knopaEnv.isConfigMode) {
		// run server
		dbg_printf(2,"Config mode\n");
		wifi_set_opmode_current(SOFTAP_MODE);
		// set up AP credentials
		struct softap_config config;
		wifi_softap_get_config(&config);
		config.channel = 1;
		os_sprintf(config.ssid, knopaConfig.apssid);
		os_sprintf(config.password, knopaConfig.appass);
		config.ssid_len = os_strlen(knopaConfig.apssid);
		config.authmode = *knopaConfig.appass ? AUTH_WPA_WPA2_PSK : AUTH_OPEN;
		config.ssid_hidden = knopaConfig.hiddenAP;
		config.max_connection = 4;
		config.beacon_interval = 100;
		os_printf("starting softAP (%s:%s) (hidden=%u)\n", knopaConfig.apssid, knopaConfig.appass, knopaConfig.hiddenAP);
		wifi_softap_set_config_current(&config);
		//
		ServerInit(80, ProcessRequest); // listen to incoming connections
	} else {
		// run normal routine
		CoreInit();
	}
}

// ============================================================================
// Scan APs callback
//
//  In:  pespconn         : connection handle
//       success          : TRUE/FALSE (general result)
//       bss_info* aps    : AP data (single-linked list)
//
//  Out: none
//
LOCAL void FLASH_CODE ScanAPs_cb (t_HttpOut* httpout, bool success, struct bss_info* aps)
{
	char *buf = NULL;
	char *p = "{\"error\":1}";
	char *auth[] = { "Open", "WEP", "WPA-PSK", "WPA2-PSK", "WPA-WPA2-PSK", "MAX" };
	char *d;

	if (success) {
		buf = (char*)k_malloc(2048, "buf", "ScanAPs_cb");
		p = buf; *p++ = '[';
		while (aps) {
			if (p-buf > 1) *p++ = ',';
			os_sprintf(p, "{\"ssid\":\"%s\",\"ch\":%u,\"rssi\":%d,\"sec\":\"%s\"}",
				aps->ssid, aps->channel, aps->rssi, auth[aps->authmode]
			);
			while (*p) p++;
			aps = (struct bss_info*)aps->next.stqe_next;
		};
		*p++ = ']';
		*p = 0;
		p = buf;

		httpout->output = buf;
		httpout->outputLength = 2048;
		HttpSend(httpout);
	} else {
		HttpError(httpout, 404);
	}
}

LOCAL void FLASH_CODE WriteConfig (char* fname, char* data)
{
	os_printf("writing file \"%s\":\n%s\n", fname, data);
	int i, size = os_strlen(data);
	t_File* f = KFS_Open(fname, "w", "application/json", size);
	if (!f) return;
	i = KFS_Write(f, data, size);
	KFS_Close(f);
	if (i != size) return;
}

LOCAL void FLASH_CODE WriteRulesBinary (char* fname)
{
	int i, size = sizeof(t_Rule)*8 + sizeof(nRules);
	os_printf("writing file \"%s\" (%d bytes)\n", fname, size);
	
	t_File* f = KFS_Open(fname, "w", "application/octet-stream", size);
	if (!f) {
		os_printf("error opening file: %s\n", KFS_ErrorMessage());
		return;
	}
	os_printf("file opened\n");
	KFS_Write(f, (char*)&nRules, sizeof(nRules));
	for (i=0; i<MAX_RULES; i++) {
		os_printf("writing rule[%d]\n", i);
		KFS_Write(f, (char*)&rules[i], sizeof(t_Rule));
	}
	os_printf("closing file...\n");
	KFS_Close(f);
	os_printf("Ok\n");
}

static char* FLASH_CODE ReadRules (void);
static bool FLASH_CODE WriteRules (char*);

// ============================================================================
// Serve user query
//
//  In:  params 			: asciizz params
//       paramCount   : number of params (key-value pairs)
//       httpout      : http connection
//
//  Out: true / false = data sent / data not sent
//
static bool FLASH_CODE ProcessAPI (char* params, int paramCount, t_HttpOut* httpout)
{
	struct { char* cmd; uint8 code; } cmdcode[] =
	{
		{"summary", 1}, // get device info
		{"status",  2}, // get wifi connection & memory status
		{"checkupd",3}, // check for update
		{"doupd",   4}, // do update
		{"sys",     5}, // save system parameters
		{"wifi",    6}, // save wifi parameters
		{"cloud",   7}, // save cloud parameters
		{"ntp",     8}, // save time parameters
		{"modules", 9}, // save installed modules
		{"reboot", 10}, // reboot
		{"scan",   11}, // scan for access points
		{"conn",   12}, // connect / disconnect
		{"rules",  13}, // read rules
		{"rconf",  14}, // save rules
		{"",0}
	};

	// convention: 1st parameter is a method!
	uint8 i=0,cmd=0;
	while (cmdcode[i].code) {
		if (GetParameterValue(params, paramCount, cmdcode[i].cmd)) { cmd = cmdcode[i].code; break; }
		i++;
	}
	if (!cmd) return false;

	char *parm, *str = NULL, *strok = "{\"result\":\"ok\"}", *strerr = "{\"result\":\"error\"}";
	char *ntps1, *ntps2, *ntps3;
	t_File* f;

	switch (cmd) {
		case 1: // summary
			SetPosixTime(atoi(GetParameterValue(params, paramCount, "sync")));
			str = DeviceInfo();
			break;
		case 2: // status
			str = DeviceStatus();
			break;
		case 3: // check fo updates
			str = "{\"result\":\"0.5a\"}";
			break;
		case 4: // do update
			str = strok;
			break;
		case 5: // save system settings
			SystemSave(
				GetParameterValue(params, paramCount, "apssid"),
				GetParameterValue(params, paramCount, "appass"),
				atoi(GetParameterValue(params, paramCount, "hidden")),
				GetParameterValue(params, paramCount, "name"),
				atoi(GetParameterValue(params, paramCount, "measure")),
				atoi(GetParameterValue(params, paramCount, "pwsave")),
				atoi(GetParameterValue(params, paramCount, "autoupd")),
				atoi(GetParameterValue(params, paramCount, "dbglvl"))
			);
			SaveConfig();
			str = strok;
			break;
		case 6: // save wifi settings
			WifiApply(
				GetParameterValue(params, paramCount, "wifissid"),
				GetParameterValue(params, paramCount, "wifipass"),
				GetParameterValue(params, paramCount, "wifibssid")
			);
			SaveConfig();
			str = strok;
			break;
		case 7: // save cloud settings
			CloudSave(
				GetParameterValue(params, paramCount, "clserv1"),
				GetParameterValue(params, paramCount, "cluser1"),
				GetParameterValue(params, paramCount, "clpass1"),
				GetParameterValue(params, paramCount, "clserv2"),
				GetParameterValue(params, paramCount, "cluser2"),
				GetParameterValue(params, paramCount, "clpass2"),
				atoi(GetParameterValue(params, paramCount, "clconn")),
				atoi(GetParameterValue(params, paramCount, "clsync"))
			);
			SaveConfig();
			str = strok;
			break;
		case 8: // save NTP settings
			SetTimeZone( CutTZ(GetParameterValue(params, paramCount, "tz")) );
			ntps1 = GetParameterValue(params, paramCount, "ntps1");
			ntps2 = GetParameterValue(params, paramCount, "ntps2");
			ntps3 = GetParameterValue(params, paramCount, "ntps3");
			SetNTPServers( ntps1, ntps2, ntps3 );
			SaveConfig();
			str = strok;
			break;
		case 9: // save (installed) modules
			WriteConfig(INSTALLED_FILE,GetParameterValue(params, paramCount, "modules"));
			str = strok;
			break;
		case 10: // reboot
			Reboot();
			str = strok;
			break;
		case 11: // scan for nearby APs
			ScanAPs(httpout, ScanAPs_cb);
			break;
		case 12: // connect / disconnect
			parm = GetParameterValue(params, paramCount, "on");
			WifiConnect( os_strcmp(parm,"0") );
			break;
		case 13: // read rules
			str = ReadRules();
			break;
		case 14: // write rules
			if (SaveRules(GetParameterValue(params, paramCount, "rconf"))) str = strok;
			else str = strerr;
			break;
		default:
			dbg_printf(3, "Unknown command: %s\n", params);
			return false; // ERROR
	}

	if (str) {
		httpout->output = str;
		httpout->outputData = strlen(str);
		httpout->outputLength = httpout->outputData;
		HttpSend(httpout);
	}
	return true;
}

static bool FLASH_CODE ProcessRequest (enRequestMethod method, char* path, char* params, int paramCount, t_HttpOut* httpout)
{
	os_printf("ProcessRequest(%s)\n", path);
	if (os_strcmp(path,"/api")==0)  return ProcessAPI(params, paramCount, httpout);
	return false;
}

// ============================================================================
//
static char* FLASH_CODE ReadRules (void)
{
	t_File *fsup, *fins, *frul;;
	fsup = KFS_Open(SUPPORTED_FILE,"r","",0);
	fins = KFS_Open(INSTALLED_FILE,"r","",0);
	frul = KFS_Open(RULES_FILE,"r","",0);
	int supsize = fsup ? fsup->size : 0;
	int inssize = fins ? fins->size : 0;
	int rulsize = frul ? frul->size : 0;

	char* rules_buf = (char*)k_malloc(64 + supsize + inssize + rulsize, "rules_buf", "ReadRules");

	char* p = rules_buf;
	p += os_sprintf (p,"{\"supported\":");
	if (fsup) p += KFS_Read(fsup, p, supsize); else p += os_sprintf(p, "[]");
	p += os_sprintf(p, ", \"installed\":");
	if (fins) p += KFS_Read(fins, p, inssize); else p += os_sprintf(p, "[]");
	p += os_sprintf(p, ", \"rules\":");
	if (frul) p += KFS_Read(frul, p, rulsize); else p += os_sprintf(p, "[]");
	p += os_sprintf(p, "}");

	KFS_Close(fsup);
	KFS_Close(fins);
	KFS_Close(frul);

	return rules_buf;
}

// ============================================================================
//
/*
typedef struct {
	uint16 cid;
	uint16 parsed; // 0, 'P'=period, 'C'=cron, '1'..'4' = number of int values
	union {
		char txt[48]; // asciizz
		int d[2], h[2], m[2], s[2]; // period from..to
		int mi, hh, dd, mm, dw, yy; // cron
		int ival[4];     // pair of values
	} params;
} t_Condition; // = 52 bytes

typedef struct {
	uint16 aid;
	uint16 parsed; // 0, 1, 2=int, 3=text, 4=int+text, 5=text+int
	char txt[48];
} t_Action; // = 52 bytes
*/
static void FLASH_CODE ParsePeriod (t_Condition* c)
{
	char delimited[64];
	uint16 i;

	CRDelimited(delimited, c->txt, 2);
	
	for (i=0; i<2; i++) {
		char* p = delimited+delimited[i];
		c->values.d[i] = (uint8)atoin(&p);
		dump_mem2(c->txt, 16);
		c->values.h[i] = (uint8)atoin(&p);
		dump_mem2(c->txt, 16);
		c->values.m[i] = (uint8)atoin(&p);
		dump_mem2(c->txt, 16);
		c->values.s[i] = (uint8)atoin(&p);
		dump_mem2(c->txt, 16);
	}
	c->parsed = 'P';
}
static void FLASH_CODE ParseCron (t_Condition* c)
{
	char* p = c->txt;
	c->values.mi = (uint8)atoin(&p);
	c->values.hh = (uint8)atoin(&p);
	c->values.dd = (uint8)atoin(&p);
	c->values.mm = (uint8)atoin(&p);
	c->values.dw = (uint8)atoin(&p);
	c->values.yy = (uint16)atoin(&p);
	c->parsed = 'C';
}
static void FLASH_CODE ParseCondition (t_Condition* c)
{
	t_Supported *s;
	uint16 i,ii,n;

	for (i=0, ii=0; i<32; i++) {
		s = &supported[i];
		if (!s->hwid) break;
		if (s->type==0) continue;
		if (ii==c->cid-10) {
			n = CountCRDelimited(s->sparams);
			break;
		}
		ii++;
	}
	c->parsed = n;
}
static void FLASH_CODE ParseRules (char* p)
{
	os_memset(rules, sizeof(rules), 0);

	char scheme[128];
	os_sprintf(scheme, "[8{%d|code/0,name/16,conditions/84,actions/292|s,s,[4{%d|cid/0,ival/4|w,[8|0|n]}],[4{%d|aid/0,parm/4|w,[8|0|n]}]}]", sizeof(t_Rule), sizeof(t_Condition), sizeof(t_Action));
	char* pp = ParseJson(p, rules, scheme);
	os_printf("Rules parsed ok\n");
	if (!pp) os_printf("%s\n", JsonError());
	nRules = 0;
	uint16 r,i;
	for (r=0; r<MAX_RULES; r++) {
		if (rules[r].actions[0].aid==0) break;
		nRules++;
		for (i=0; i<MAX_CONDITIONS; i++) {
			if (rules[r].conditions[i].cid==0) break;
			os_printf("cid = %d\n", rules[r].conditions[i].cid);
			switch (rules[r].conditions[i].cid) {
				case 1: ParsePeriod(&rules[r].conditions[i]); break;
				case 2: ParseCron(&rules[r].conditions[i]); break;
				default: ParseCondition(&rules[r].conditions[i]); break;
			}
		}
	}
}

// ============================================================================
//
/*

typedef struct {
	uint8 type; // int, decimal, string
	int32 ivalue;
} t_ModuleParam; // 5

typedef struct {
	uint16 hwid;              // 2
	uint16 type;              // 2
	char sproto[16];          // 16
	char sparams[128];        // 128
	uint16 proto;             // 2
	t_ModuleParam params[4];  // 4*6
} t_Supported; //


*/
static void FLASH_CODE ReadSupported (void)
{
	t_File* f = KFS_Open("supported.json", "r", "", 0);
	if (!f) return;
	char* buf = (char*)k_malloc(f->size+2, "buf", "ReadSupported");
	if (!f) { KFS_Close(f); return; }
	int i = KFS_Read(f, buf, f->size);
	buf[f->size] = 0; // asciiz
	KFS_Close(f);
	os_memset(supported, sizeof(supported), 0);
	char scheme[80];
	os_sprintf(scheme, "[32{%d|hwid/0,type/2,protocol/4,params/20,units/20|w,w,s,[4|0|n],[4|0|n]}]", sizeof(t_Supported));
	os_printf(scheme);
	char* pp = ParseJson(buf, supported, scheme);
	if (!pp) os_printf("%s\n", JsonError());
	k_free(buf);
	return;
}

// ============================================================================
//
static bool FLASH_CODE SaveRules (char* p)
{
	int i;
	WriteConfig(RULES_FILE, p);
	//if (supported[0].hwid==0)
		ReadSupported();
	ParseRules(p);
	WriteRulesBinary(RULES_FILE_BIN);
	return true;
}

// ============================================================================
//
static void FLASH_CODE Debug_HardwareConfig (void)
{
	char flags[4];
	if (spi_flash_read(0, (uint32*)flags, 4) != SPI_FLASH_RESULT_OK) { os_printf("  spi!\n"); return; }
	os_printf("Flash header = %02X %02X %02X %02X\n", flags[0], flags[1], flags[2], flags[3]);
	os_printf("Flash ID = %08X\n", spi_flash_get_id());
	os_printf("Sys sectors at %X000\n",user_rf_cal_sector_set());
	os_printf("Heap: %d\n", system_get_free_heap_size());
}

// ============================================================================
//
//
static void FLASH_CODE SaveConfig (void)
{
	t_File* f = KFS_Open("knopa.config", "w", "application/x-knopa-config", sizeof(knopaConfig));
	if (!f) { os_printf("File error (1): %s\n", KFS_ErrorMessage()); }
	else {
		if (!KFS_Write(f, (char*)&knopaConfig, sizeof(knopaConfig))) { os_printf("File error (2): %s\n", KFS_ErrorMessage()); };
	}
	os_printf("Config saved\n");
	KFS_Close(f);
}

