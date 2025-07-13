
#include <user_interface.h>	// station_config
#include <osapi.h>			// os_sprintf
#include <ip_addr.h>		// ip_info

#include "knopa_misc.h"
#include "knopa_config.h"
#include "knopa_time.h"
#include "knopa_dbg.h"
#include "knopa_sys.h"
#include "knopa_fs.h"

// ============================================================================

void FLASH_CODE dump_mem (char* msg, const char* data, uint32 len, uint32 size, uint32 lenz)
{
	os_printf("%s: %u bytes [ ", msg, size);
	uint8 x = 0;
	const char *p = data;
	while (len-- > 0) os_printf("%02X ", *p++);
	if (size) {
		os_printf("... ");
		while (lenz > 0) os_printf("%02X ", data[size-(lenz--)]);
	}
	os_printf("]\n");
}

// ============================================================================

void FLASH_CODE dump_mem2 (const char* data, uint32 len)
{
	uint8 x = 0;
	const char *p = data;
	while (len-- > 0) os_printf("%02X ", *p++);
	os_printf("\n");
}

// ============================================================================
// Get general device info as a json string
//
char* FLASH_CODE DeviceInfo (void)
{
	// tz 			 : -11*60..13*60 (minutes)
	// pwsave 	 : 1 / 0
	// sleeptime : 0..4290
	//

	char* device_info_buf = NULL;

	// debug info ------------------
	uint32 userbin_addr = system_get_userbin_addr(); // (convert to hex)
	os_printf("userbin_addr = %08X\n", userbin_addr);
	uint8 boot_mode = system_get_boot_mode(); 			 // 0=ENHANCED, 1=NORMAL
	os_printf("boot_mode = %X\n", boot_mode);
	system_print_meminfo (); // DEBUG

	// collect data ----------------
	uint32 heap_free = system_get_free_heap_size();  // bytes
	char* sdk_ver = (char*)system_get_sdk_version(); // 1.5.2
	uint8 boot_ver = system_get_boot_version(); 		 // >=3 ? enhanced boot mode is available
	uint8 cpu_freq = system_get_cpu_freq(); 				 // MHz
	uint8 flash_sz = system_get_flash_size_map(); 	 // see enum flash_size_map
	char ntps[256], clouds[256];
	sint16 tz = GetTimeZone();
	CRDelimited(ntps, knopaConfig.ntp_servers, 3);
	CRDelimited(clouds, knopaConfig.cloud, 6);

	t_File *fsup, *fins;
	fsup = KFS_Open("supported.json","r","",0);
	fins = KFS_Open("installed.json","r","",0);
	int supsize = fsup ? fsup->size : 0;
	int inssize = fins ? fins->size : 0;
	
	char* flash_size = NULL;
	switch (flash_sz) {
		case FLASH_SIZE_2M: flash_size = "2 Mbit"; break;
		case FLASH_SIZE_4M_MAP_256_256: flash_size = "4 Mbit (256+256)"; break;
		case FLASH_SIZE_8M_MAP_512_512: flash_size = "8 Mbit (512+512)"; break;
		case FLASH_SIZE_16M_MAP_512_512: flash_size = "16 Mbit (512+512)"; break;
		case FLASH_SIZE_32M_MAP_512_512: flash_size = "32 Mbit (512+512)"; break;
		case FLASH_SIZE_16M_MAP_1024_1024: flash_size = "16 Mbit (1024+1024)"; break;
		case FLASH_SIZE_32M_MAP_1024_1024: flash_size = "32 Mbit (1024+1024)"; break;
		default: flash_size = "Unknown flash map"; break;
	}

	// format and print out json -----------------

	device_info_buf = k_malloc(1024 + supsize + inssize, "device_info_buf", "DeviceInfo");

	char* p = device_info_buf;
	p += os_sprintf (device_info_buf,
		"{\"fver\":\"%s\","
		"\"devinfo\":\"%u MHz, %s\","
		"\"sdkboot\":\"%s + %u\","
		"\"heap\":\"%d\","
		"\"ntps1\":\"%s\","
		"\"ntps2\":\"%s\","
		"\"ntps3\":\"%s\","
		"\"tz\":\"%d\","
		"\"uptime\":\"%u\","
		"\"wifissid\":\"%s\","
		"\"wifipass\":\"%s\","
		"\"wifibssid\":\"%s\","
		"\"apssid\":\"%s\","
		"\"appass\":\"%s\","
		"\"hidden\":\"%u\","
		"\"name\":\"%s\","
		"\"measure\":%u,"
		"\"pwsave\":%u,"
		"\"autoupd\":%u,"
		"\"dbglvl\":%u,"
		"\"clserv1\":\"%s\","
		"\"cluser1\":\"%s\","
		"\"clpass1\":\"%s\","
		"\"clserv2\":\"%s\","
		"\"cluser2\":\"%s\","
		"\"clpass2\":\"%s\","
		"\"clconn\":\"%u\","
		"\"clsync\":\"%u\","
		"\"supported\":",
		KNOPA_VERSION,
		cpu_freq, flash_size,
		sdk_ver, boot_ver,
		heap_free,
		ntps+ntps[0],
		ntps+ntps[1],
		ntps+ntps[2],
		tz,
		GetUptime(),
		knopaConfig.wifissid,
		knopaConfig.wifipass,
		knopaConfig.wifibssid,
		knopaConfig.apssid,
		knopaConfig.appass,
		knopaConfig.hiddenAP,
		knopaConfig.name,
		knopaConfig.measure,
		knopaConfig.powersave,
		knopaConfig.autoupd,
		knopaConfig.dbglvl,
		clouds+clouds[0],
		clouds+clouds[1],
		clouds+clouds[2],
		clouds+clouds[3],
		clouds+clouds[4],
		clouds+clouds[5],
		knopaConfig.clconn,
		knopaConfig.clsync
	);

	if (fsup) p += KFS_Read(fsup, p, supsize); else p += os_sprintf(p, "[]");
	p += os_sprintf(p, ", \"modules\":");
	if (fins) p += KFS_Read(fins, p, inssize); else p += os_sprintf(p, "[]");
	p += os_sprintf(p, "}");
	KFS_Close(fsup);
	KFS_Close(fins);

	return device_info_buf;
}

// ============================================================================
// Get WiFi status as a json string
//
char* FLASH_CODE DeviceStatus (void)
{
	char* device_status_buf = k_malloc(128, "status_buf", "DeviceStatus");
	char buf[64];
	char* status[] = { "Not connected", "Connecting...", "%s: wrong password", "%s not found", "%s: connect failed", "Connected to %s", "Station mode is off" };
	struct station_config config;
	wifi_station_get_config(&config);
	uint8 stat = wifi_station_get_connect_status();
	struct ip_info ipinfo;
	wifi_get_ip_info(STATION_IF, &ipinfo);
	if (stat>5) stat = 6;
	os_sprintf(buf, status[stat], config.ssid);
	os_sprintf(device_status_buf, "{\"apstatus\":\"%s\",\"heap\":%d,\"stationip\":\""IPSTR"\"}", buf, system_get_free_heap_size(), IP2STR(&ipinfo.ip) );

	return device_status_buf;
}

// ============================================================================

void FLASH_CODE Reboot (void)
{
	os_printf("Rebooting...\n");
	system_restart();
}

// ============================================================================

void FLASH_CODE CloudSave (char* srv1, char* user1, char* pass1, char* srv2, char* user2, char* pass2, uint8 clconn, uint8 clsync)
{
	char* p = knopaConfig.cloud;
	while (*p++ = *srv1++) ;
	while (*p++ = *user1++) ;
	while (*p++ = *pass1++) ;
	while (*p++ = *srv2++) ;
	while (*p++ = *user2++) ;
	while (*p++ = *pass2++) ;
	knopaConfig.clconn = clconn;
	knopaConfig.clsync = clsync;
}

// ============================================================================

void FLASH_CODE SystemSave (
				 char* apssid,
				 char* appass,
				 uint8 hidden,
				 char* name,
				 uint16 measure,
				 uint8 pwsave,
				 uint8 autoupd,
				 uint8 dbglvl
			 )
{
	os_strcpy(knopaConfig.apssid, apssid);
	os_strcpy(knopaConfig.appass, appass);
	os_strcpy(knopaConfig.name, name);
	knopaConfig.measure = measure;
	knopaConfig.powersave = pwsave;
	knopaConfig.autoupd = autoupd;
	knopaConfig.dbglvl = dbglvl;
	knopaConfig.hiddenAP = hidden;
}

// ============================================================================

