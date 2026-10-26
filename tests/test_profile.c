/* Deterministic accounting tests: nested work, maxima, callbacks, cadence. */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "../source/hcsnes_profile.h"
static uint64_t now = 1;
static unsigned reports, reads;
static char line[2048];
uint64_t hcsnes_profile_test_clock(void) { ++reads; return now; }
static void logger(enum retro_log_level level, const char *fmt, ...)
{
   va_list ap; (void)level;
   va_start(ap, fmt); vsnprintf(line, sizeof(line), fmt, ap); va_end(ap);
   if (strstr(line, "HCSNES profile ")) ++reports;
}
int main(void)
{
   unsigned i;
   hcsnes_profile_configure(false, logger);
   reads = 0;
   hcsnes_profile_begin_frame();
   HCSNES_PROFILE_ENTER(HCP_CPU);
   HCSNES_PROFILE_LEAVE();
   hcsnes_profile_end_frame(true,0,1,false,0,false);
   assert(reads == 0 && reports == 0);
   hcsnes_profile_configure(true, NULL);
   assert(!hcsnes_profile_enabled);
   hcsnes_profile_configure(true, logger);
   for (i=0;i<300;++i) {
      hcsnes_profile_begin_frame();
      now += 1000; HCSNES_PROFILE_ENTER(HCP_CPU);
      now += 2000; HCSNES_PROFILE_ENTER(HCP_GSU);
      now += 3000; HCSNES_PROFILE_LEAVE();
      now += 4000; HCSNES_PROFILE_ENTER(HCP_PPU);
      now += 5000; HCSNES_PROFILE_LEAVE();
      HCSNES_PROFILE_LEAVE();
      HCSNES_PROFILE_ENTER(HCP_APU);
      now += 6000; HCSNES_PROFILE_ENTER(HCP_APU);
      now += 1000; HCSNES_PROFILE_LEAVE();
      HCSNES_PROFILE_LEAVE();
      HCSNES_PROFILE_ENTER(HCP_VIDEO);
      now += 9000; HCSNES_PROFILE_LEAVE();
      HCSNES_PROFILE_ENTER(HCP_AUDIO);
      now += 10000; HCSNES_PROFILE_LEAVE();
      hcsnes_profile_gsu_counts(7,2,1);
      hcsnes_profile_end_frame((i%2)==0,3,1,true,50,(i%3)==0);
      assert(reports == (i==299 ? 1u : 0u));
   }
   assert(strstr(line,"n=300 reason=window"));
   assert(strstr(line,"rendered=150 skipped=150"));
   assert(strstr(line,"feedback=300 underruns=100"));
   assert(strstr(line,"core=22/22 cpu_other=6/6 gsu=3/3 ppu=5/5 apu=7/7"));
   assert(strstr(line,"video_cb=9/9 audio_cb=10/10"));
   assert(strstr(line,"gsu_inst=2100 plot=600 rpix=300"));
   hcsnes_profile_begin_frame();
   now+=8000;
   hcsnes_profile_end_frame(true,0,1,false,0,false);
   hcsnes_profile_configure(false,logger);
   assert(reports==2 && strstr(line,"n=1 reason=off"));
   assert(strstr(line,"core=8/8") && strstr(line,"gsu_inst=0"));
   hcsnes_profile_configure(true,logger);
   hcsnes_profile_begin_frame();
   now = 0; /* Timer failure cannot become an unsigned huge duration. */
   HCSNES_PROFILE_ENTER(HCP_CPU);
   hcsnes_profile_end_frame(true,0,1,false,0,false);
   assert(!hcsnes_profile_enabled);
   puts("PASS: profiler exclusive nesting, frontend separation, cadence, maxima, disabled path, clock failure");
   return 0;
}
