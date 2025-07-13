
#include "knopa_sys.h"
#include <osapi.h> // os_strcpy, os_printf

typedef struct {
	char name[22];
	char func[32];
	int  size;
	char* ptr;
	char action;
} memallocated;

#define MAX_ALLOC 32
static memallocated mallocd[MAX_ALLOC];
static int nmallocd = 0;

char* FLASH_CODE k_malloc (int size, char* name, char* func)
{
	os_strcpy(mallocd[nmallocd].name, name);
	os_strcpy(mallocd[nmallocd].func, func);
	mallocd[nmallocd].size = size;
	mallocd[nmallocd].ptr = (char*)os_malloc(size);
	mallocd[nmallocd].action = '+';
	nmallocd++;
	k_memview();
	mallocd[nmallocd-1].action = ' ';
	return mallocd[nmallocd-1].ptr;
}

void FLASH_CODE k_free (char* ptr)
{
	int i,x = -1;
	for (i=0; i<MAX_ALLOC; i++) {
		if (mallocd[i].ptr == ptr) { x = i; break; }
	}
	if (x >= 0) {
		os_free(mallocd[x].ptr);
		mallocd[x].ptr = 0;
		mallocd[x].action = '-';
		k_memview();
		for (i=x; i<MAX_ALLOC-1; i++)
			mallocd[i] = mallocd[i+1];
		nmallocd--;
	} else {
		os_printf("+++++++++++++ not found: %08X\n", ptr);
	}
}

void FLASH_CODE k_memview (void)
{
//	return; // <-----------------------------------
	int i, pad, v;
	/*
	char sp[5] = "    ";
	os_printf("-------------\n");
	for (i=0; i<nmallocd; i++) {
		pad = 0;
		v = mallocd[i].size;
		while (v) { pad++; v = v/10; }
		os_printf("  %s%d %s %s at %s\n", sp+pad, mallocd[i].size, (mallocd[i].action=='+' ? "NEW" : (mallocd[i].action=='-' ? "DEL" : "---")), mallocd[i].name, mallocd[i].func);
	}
	os_printf("----FREE: %d\n", system_get_free_heap_size());
	*/
	for (i=0; i<nmallocd; i++) {
		if (mallocd[i].action!=' ') {
			os_printf("%s %d : %s at %s (%d free)\n", (mallocd[i].action=='+' ? "+++" : (mallocd[i].action=='-' ? "---" : "")), mallocd[i].size, mallocd[i].name, mallocd[i].func, system_get_free_heap_size());
			break;
		}
	}

}
