#include <assert.h>
#include <stdio.h>
#include "../source/hcsnes_rtc.h"
int main(void)
{
   uint8_t b[10]={0xa5};
   uint64_t values[]={0,1,0x7fffffff,0x80000000,0x0123456789abcdefULL,UINT64_MAX};
   unsigned i;
   b[9]=0x5a;
   for(i=0;i<sizeof(values)/sizeof(values[0]);++i){
      hcsnes_rtc_store(b+1,values[i]);
      assert(hcsnes_rtc_load(b+1)==values[i]);
      assert(b[0]==0xa5 && b[9]==0x5a);
   }
   hcsnes_rtc_store(b+1,0x0123456789abcdefULL);
   assert(b[1]==0xef && b[8]==0x01);
   puts("PASS: RTC fixed-width encoding, endian order and boundary guards");
   return 0;
}
