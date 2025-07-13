// ============================================================================

#include "kmem.h"
#include "knopa_dbg.h"

void FLASH_CODE dbg_printa(uint8 lvl, char* str)
{
  if (lvl > knopaConfig.dbglvl) return;
  os_printf("{");
  int i = 0;
  while (*str) {
    os_printf("%s\"%s\"", i?",":"", str);
    while (*str++) ;
    i++;
  };
  os_printf("}\n");
}

