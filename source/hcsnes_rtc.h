/* HCSNES 2026-10-25: portable fixed-width RTC save encoding. */
#ifndef HCSNES_RTC_H
#define HCSNES_RTC_H
#include <stdint.h>
static void hcsnes_rtc_store(uint8_t *dst, uint64_t stamp)
{
   unsigned i;
   for(i=0;i<8;++i) dst[i]=(uint8_t)(stamp>>(8*i));
}
static uint64_t hcsnes_rtc_load(const uint8_t *src)
{
   uint64_t stamp=0;
   unsigned i;
   for(i=0;i<8;++i) stamp|=(uint64_t)src[i]<<(8*i);
   return stamp;
}
#endif
