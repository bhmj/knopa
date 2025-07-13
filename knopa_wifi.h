#ifndef _KNOPA_WIFI_H
#define _KNOPA_WIFI_H

#include "kmem.h"
#include <c_types.h>
#include <ip_addr.h>
#include <espconn.h>
#include <user_interface.h>
#include "knopa_server.h"

typedef void (* Scan_APs_cb_t) (t_HttpOut* httpout, bool success, struct bss_info* aps);

void   FLASH_CODE ScanAPs (t_HttpOut* httpout, Scan_APs_cb_t cb);

bool   FLASH_CODE WifiApply (char* ssid, char* pass, char* bssid);

bool   FLASH_CODE WifiConnect (bool on);

#endif
