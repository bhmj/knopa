
#include "knopa_time.h"
#include "knopa_config.h"
#include "knopa_dbg.h"

#include <os_type.h>
#include <osapi.h>

// ============================================================================

static volatile os_timer_t upTimer;

#define GPIO2 BIT2
#define GPIO4 BIT4
#define GPIO5 BIT5

#define NLEDs 3

static uint32 LED_channel[NLEDs] = {GPIO2, GPIO4, GPIO5}; // HARDCODED AT THE MOMENT

volatile uint8 LED_cycle[NLEDs] = { 0, 0, 0};   // LED cycle duration in ticks
volatile uint8 LED_duty [NLEDs] = { 0, 0, 0};   // LED duty in ticks
volatile uint8 LED_count[NLEDs] = { 0, 0, 0};   // (internal counter)
volatile uint8 LED_state[NLEDs] = { 0, 0, 0};   // (internal state storage)

volatile uint32 uptimer_cycle = 0; // 10 times
volatile uint32 uptime = 0;        // seconds

// ============================================================================
//
//
void FLASH_CODE InitTime (void)
{
}

// ============================================================================
//
//
uint32 FLASH_CODE GetPosixTime (void)
{
  return knopaConfig.posix_time;
}

// ============================================================================
//
//
void FLASH_CODE SetPosixTime (uint32 p_time)
{
  knopaConfig.posix_time = p_time;
}

// ============================================================================
// In: {s} -- timezone in string format (ex: GMT-5:45)
//
sint16 FLASH_CODE CutTZ (char* s)
{
  sint16 tzh=0, tzm=0;
  uint8 sign=0, dot=0;
  while (*s) {
    char ch = *s++;
    if (ch=='-') sign = 1; // minus
    if (ch>='0' && ch<='9') tzm = tzm*10 + (ch-'0');
    if (ch==':') { dot++; tzh = tzm*60; tzm = 0; }
  }
  if (!dot && tzm<=13) { tzh = tzm*60; tzm = 0; }
  tzm += tzh;
  return sign ? -tzm : tzm;
}

// ============================================================================
//
//
sint16 FLASH_CODE GetTimeZone (void)
{
  return knopaConfig.timezone;
}

// ============================================================================
// In: {tz} -- valid timezone in seconds
//
void FLASH_CODE SetTimeZone (sint16 tz)
{
  if (tz>=-12*60 && tz<=13*60)
    knopaConfig.timezone = tz;
}

// ============================================================================
// In: {srv1},{srv2},{srv3} -- asciiz server names or IP4
//
bool FLASH_CODE SetNTPServers (char* srv1, char* srv2, char* srv3)
{
  uint16 len = 0;
  uint8 i;
  char *p = knopaConfig.ntp_servers;
  if (os_strlen(srv1)>47) return false;
  if (os_strlen(srv2)>47) return false;
  if (os_strlen(srv3)>47) return false;
  while (*srv1) *p++ = *srv1++; *p++ ='\n';
  while (*srv1) *p++ = *srv1++; *p++ ='\n';
  while (*srv1) *p++ = *srv1++; *p++ ='\n';

  return true;
}

// ============================================================================
//
//
uint8 FLASH_CODE SyncTime (uint8 attempts, uint8 interval)
{
}

// ============================================================================
// RTC timer
//
void FLASH_CODE upTimer_cb (void* arg) // 0.1 sec
{
  int i;

  for (i=0; i<NLEDs; i++) {
    uint8 cycle = LED_cycle[i]; // 0..n
    uint8 count = LED_count[i]; // 0..n
    uint8 duty  = LED_duty[i];  // 0..n
    uint8 on = (count < duty ? 1 : 0);
    if (++count >= cycle) count = 0;
    LED_count[i] = count;
    if (LED_state[i] != on) {
      uint32 bit = LED_channel[i];
      gpio_output_set((1-on)*bit, on*bit, bit, 0);
      LED_state[i] = on;
    }
  }
  if (++uptimer_cycle==10) { // 1 sec
    uptimer_cycle = 0;
    uptime++;
    knopaConfig.posix_time++;
  }
  if (!uptimer_cycle && uptime%60==0) { // once in a minute
//    dbg_printf(4, "Uptime: %02u:%02u:%02u\n", (uptime/3600)%60, (uptime/60)%60, uptime%60);
  }
}

// ============================================================================
//
//
void FLASH_CODE RunRTCTimer (void)
{
  os_timer_disarm(&upTimer);
  os_timer_setfn(&upTimer, (os_timer_func_t*)upTimer_cb, NULL);
  os_timer_arm(&upTimer, 100, 1);
}

// ============================================================================
//
//
uint32 FLASH_CODE GetUptime (void)
{
  return uptime;
}

// ============================================================================
//
//
void FLASH_CODE SetBlinky (uint8 channel, uint8 cycle, uint8 duty)
{
  if (channel > 2) return;
  LED_count[channel] = 0;
  LED_duty[channel]  = duty;
  LED_cycle[channel] = cycle;
}
