#ifndef _KNOPA_SYS_H
#define _KNOPA_SYS_H

#include "kmem.h"
#include <mem.h> // os_malloc

char* FLASH_CODE k_malloc (int size, char* name, char* func);
void  FLASH_CODE k_free (char* ptr);
void  FLASH_CODE k_memview (void);

#define OFFSETOF(type, field)    ((unsigned long) &(((type *) 0)->field))

#endif
