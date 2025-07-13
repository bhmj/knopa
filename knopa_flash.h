#ifndef _KNOPA_FLASH_H
#define _KNOPA_FLASH_H

#include "kmem.h"
#include <spi_flash.h>

bool FLASH_CODE Storage_ReadParams (void *param, uint16 len);

bool FLASH_CODE Storage_WriteParams (void *param, uint16 len);

#endif
