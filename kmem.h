#ifndef _KMEM_H
#define _KMEM_H

#include <c_types.h>

//#define FLASH_CODE __attribute__((section(".irom2.text")))
//#define RAM_CODE   __attribute__((section(".text")))
#define FLASH_CODE __attribute__((section(".irom0.text")))
#define RAM_CODE   __attribute__((section(".text")))

#endif
