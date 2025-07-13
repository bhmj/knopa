#ifndef _KNOPA_TIME_H
#define _KNOPA_TIME_H

#include "kmem.h"
#include <c_types.h>

uint32 FLASH_CODE GetPosixTime (void);

void   FLASH_CODE SetPosixTime (uint32);

sint16 FLASH_CODE CutTZ (char* s);

sint16 FLASH_CODE GetTimeZone (void);

void   FLASH_CODE SetTimeZone (sint16);

bool   FLASH_CODE SetNTPServers (char* srv1, char* srv2, char* srv3);

void   FLASH_CODE GetNTPServers (char* servers);

uint8  FLASH_CODE SyncTime (uint8 attempts, uint8 interval);

void   FLASH_CODE RunRTCTimer (void);

uint32 FLASH_CODE GetUptime (void);

void   FLASH_CODE SetBlinky (uint8 pin, uint8 cycle, uint8 duty);

#endif
