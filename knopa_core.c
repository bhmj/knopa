
#include "knopa_core.h"

typedef struct {
	uint32 target_sec;
	uint32 current_sec;
} t_CoreData;

static t_CoreData coreData;

static volatile os_timer_t coreTimer;

// ============================================================================
// Logic
//
void FLASH_CODE Activate (void)
{
	//
}

// ============================================================================
// Read RTC data with CRC
//
static bool FLASH_CODE ReadCoreData (void)
{
	char buf[sizeof(t_CoreData)+4];
	bool b = system_rtc_mem_read(64, buf, sizeof(t_CoreData)+4);
	uint32 crc = crc32c(buf, sizeof(t_CoreData));
	if ( *(uint32*)(buf+sizeof(t_CoreData)) != crc) return false;
	else os_memcpy(coreData, buf, sizeof(t_CoreData));
	return true;
}

// ============================================================================
// Write RTC data with CRC
//
static void FLASH_CODE WriteCoreData (void)
{
	char buf[sizeof(t_CoreData)+4];
	os_memcpy(buf, coreData, sizeof(t_CoreData));
	uint32 crc = crc32c(buf, sizeof(t_CoreData));
	*(uint32*)(buf+sizeof(t_CoreData)) = crc;
	system_rtc_mem_write(64, buf, sizeof(t_CoreData)+4);
}

// ============================================================================
// Core timer
//
void FLASH_CODE coreTimer_cb (void* arg)
{
	if (coreData.current_sec >= coreData.target_sec) {
		Activate();
	}
}

// ============================================================================
//
//
static void FLASH_CODE RunCoreTimer (void)
{
	os_timer_disarm(&coreTimer);
	os_timer_setfn(&coreTimer, (os_timer_func_t*)coreTimer_cb, NULL);
	os_timer_arm(&coreTimer, 100, 1);
}


void   FLASH_CODE CoreInit (void)
{
	bool activate = true;
	if (knopaConfig.powersave) {
		if (ReadCoreData()) {
			if (coreData.current_sec < coreData.target_sec) activate = false;
		} else {
			//coreData.target_sec = knopaConfig.
		}
	}

	if (activate) Activate();
	else {
		// deep sleep or arm timer
	}
}

