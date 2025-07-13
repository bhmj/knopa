#ifndef _KNOPA_CONFIG_H
#define _KNOPA_CONFIG_H

#include "kmem.h"
#include <c_types.h>
#include "knopa_wifi.h"
#include "knopa_server.h"

#define KNOPA_VERSION "0.33a"

#define N_PINS 16

// Must be aligned to 4 bytes!
typedef struct {
  char   magic[16];          // "knopa.online" asciiz
  // system
  char   apssid[32];         // asciiz
  char   appass[48];         // asciiz
  char   name[32];           // asciiz
  uint32 measure;            // 0..65500
  uint8  powersave;          // 1 = use sleep cycles, 0 = always on
  uint8  autoupd;            // 1 = check for updates and install automatically, 0 = manual update
  uint8  dbglvl;             // 0..4 (none | procs | key data | most | total)
  // Time
  uint32 posix_time;         // unix timestamp (GMT)
  sint16 timezone;           // timezone in minutes (ex: +180 = GMT+3:00, -330 = GMT-5:30)
  char   ntp_servers[48*3];  // \n-separated ASCIIZ string
  // Wifi
  char   wifissid[32];       //
  char   wifipass[48];       //
  char   wifibssid[48];      //
  uint8  hiddenAP;           //
  // Cloud
  char   cloud[256];         //
  uint8  clconn;             //
  uint16 clsync;             //
  // Hardware types
  char   hardware[512];      // hardware (\n-delimited)
  //
  uint8  pad[1];             // padding to 4 bytes
} t_KnopaConfig;

// -------------------------------------------------------------------------

typedef struct {
  uint8 opmode;              //
  uint8 isConfigMode;        // config mode
  t_HttpOut* httpout;        // connection
  Scan_APs_cb_t scan_cb;     // AP scan callback fn
} t_KnopaEnv;

// -------------------------------------------------------------------------

extern t_KnopaConfig knopaConfig;

extern t_KnopaEnv knopaEnv;

// ============================================================================
// Init Knopa sub-system, use {config_gpio} as config button
//
void FLASH_CODE KnopaInit (uint8 config_gpio);

#endif
