
#include <osapi.h> // os_strcpy

#include "knopa_wifi.h"
#include "knopa_config.h"
#include "knopa_dbg.h"

void FLASH_CODE Wifi_Scan_cb (void *arg, STATUS status)
{
//  wifi_set_opmode(knopaEnv.opmode);
  if (status==OK) {
    struct bss_info *bss_link = (struct bss_info *)arg;
    knopaEnv.scan_cb(knopaEnv.httpout, TRUE, bss_link);
  } else {
    knopaEnv.scan_cb(knopaEnv.httpout, FALSE, NULL);
  }
}

void FLASH_CODE ScanAPs (t_HttpOut* httpout, Scan_APs_cb_t cb)
{
  knopaEnv.httpout = httpout;
  knopaEnv.scan_cb = cb;
  knopaEnv.opmode = wifi_get_opmode(); // 1=station, 2=softAP, 3=station+softAP
  wifi_set_opmode(knopaEnv.opmode | STATION_MODE);
  bool b = wifi_station_scan (NULL, Wifi_Scan_cb);
  if (!b) cb(httpout, false, NULL);
}

bool FLASH_CODE WifiApply (char* ssid, char* pass, char* bssid)
{
  uint16 plen = os_strlen(pass);
  os_strcpy(knopaConfig.wifissid,  ssid);
  if (plen >= 8 || plen==0) os_strcpy(knopaConfig.wifipass,  pass);
  os_strcpy(knopaConfig.wifibssid, bssid);
  WifiConnect(1);
}

bool FLASH_CODE WifiConnect (bool on)
{
  bool b;
  uint8 opmode = wifi_get_opmode(); // 1=station, 2=softAP, 3=station+softAP

  if (on) {
    dbg_printf(2, "opmode=%u\n",opmode);
    wifi_set_opmode(opmode | STATION_MODE);
    struct station_config sta_conf;
    os_memset(sta_conf.ssid, 0, 32);
    os_memset(sta_conf.password, 0, 64);
    os_memset(sta_conf.bssid, 0, 6);
    os_strcpy(sta_conf.ssid, knopaConfig.wifissid);
    os_strcpy(sta_conf.password, knopaConfig.wifipass);
    os_strncpy(sta_conf.bssid, knopaConfig.wifibssid, 5); sta_conf.bssid[5]=0;
    sta_conf.bssid_set = sta_conf.bssid[0] ? 1 : 0;
    b = wifi_station_set_config_current(&sta_conf);
    if (!b) dbg_printf(2, "station set FAILED\n");
    b = wifi_station_connect();
    if (!b) dbg_printf(2, "wifi connect FAILED\n");
  } else {
    b = wifi_station_disconnect();
    if (!b) dbg_printf(2, "wifi disconnect FAILED\n");
    wifi_set_opmode(opmode & (~STATION_MODE));
  }
  return b;
}
