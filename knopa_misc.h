#ifndef _KNOPA_MISC_H
#define _KNOPA_MISC_H

#include "kmem.h"
#include <c_types.h>

uint32 FLASH_CODE crc32c (char *data, uint16 length);

void FLASH_CODE dump_mem (char* msg, const char* data, uint32 len, uint32 size, uint32 lenz);

char*  FLASH_CODE  DeviceInfo (void);

void   FLASH_CODE FreeDeviceInfo (void);

char*  FLASH_CODE DeviceStatus (void);

void   FLASH_CODE FreeDeviceStatus (void);

void   FLASH_CODE Reboot (void);

void   FLASH_CODE CloudSave (
         char* srv1,
         char* user1,
         char* pass1,
         char* srv2,
         char* user2,
         char* pass2,
         uint8 clconn,
         uint8 clsync
       );

void   FLASH_CODE SystemSave (
         char* apssid,
         char* appass,
         uint8 hidden,
         char* name,
         uint16 measure,
         uint8 pwsave,
         uint8 autoupd,
         uint8 dbglvl
       );

char*  FLASH_CODE SavePins (char* pins, char* types);

char*  FLASH_CODE ReadPinConfig (uint8 pin);

void   FLASH_CODE FreePinConfig (void);

sint32 FLASH_CODE atoi (char* s);

#endif
