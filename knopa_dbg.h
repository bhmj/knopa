#ifndef _KNOPA_DBG_H
#define _KNOPA_DBG_H

#include "kmem.h"
#include <osapi.h> // os_strcpy
#include <c_types.h>
#include <stdio.h>

#include "knopa_config.h"
#include "knopa_server.h"

void FLASH_CODE dbg_printa(uint8 lvl, char* str);
// void FLASH_CODE dbg_printp(uint8 lvl, t_RequestParam *params);
//void dbg_printf(uint8 lvl, char* str, ...);

#define dbg_printf(lvl, fmt, ...) if (lvl <= knopaConfig.dbglvl) {  \
  static const char flash_str##__LINE__[] ICACHE_RODATA_ATTR STORE_ATTR = fmt;  \
  os_printf(flash_str##__LINE__, ##__VA_ARGS__); \
  }

#endif