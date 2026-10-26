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
      now += (i % 2) == 0 ? 5000 : 1000; HCSNES_PROFILE_LEAVE();
      HCSNES_PROFILE_LEAVE();
      HCSNES_PROFILE_ENTER(HCP_APU);
      now += 6000; HCSNES_PROFILE_ENTER(HCP_APU);
      now += 1000; HCSNES_PROFILE_LEAVE();
      HCSNES_PROFILE_LEAVE();
      HCSNES_PROFILE_ENTER(HCP_VIDEO);
      now += 9000; HCSNES_PROFILE_LEAVE();
      HCSNES_PROFILE_ENTER(HCP_AUDIO);
      now += 10000; HCSNES_PROFILE_LEAVE();
      HCSNES_PROFILE_ENTER(HCP_INPUT);
      now += 2000; HCSNES_PROFILE_LEAVE();
      hcsnes_profile_gsu_counts(7,2,1);
      hcsnes_profile_end_frame((i%2)==0,3,1,true,50,(i%3)==0);
      assert(reports == (i==299 ? 1u : 0u));
   }
   assert(strstr(line,"n=300 reason=window"));
   assert(strstr(line,"rendered=150 skipped=150"));
   assert(strstr(line,"feedback=300 underruns=100"));
   assert(strstr(line,"core=20/22 cpu_other=6/6 gsu=3/3 ppu=3/5 apu=7/7"));
   assert(strstr(line,"video_cb=9/9 audio_cb=10/10 input_cb=2/2"));
   assert(strstr(line,"gsu_inst=2100 plot=600 rpix=300"));
   assert(strstr(line,"core_drawn=22/22 core_skipped=18/18"));
   hcsnes_profile_begin_frame();
   now+=8000;
   hcsnes_profile_end_frame(true,0,1,false,0,false);
   hcsnes_profile_configure(false,logger);
   assert(reports==2 && strstr(line,"n=1 reason=off"));
   assert(strstr(line,"core=8/8") && strstr(line,"gsu_inst=0"));
   assert(strstr(line,"core_drawn=8/8 core_skipped=0/0"));
   hcsnes_profile_configure(true,logger);
   hcsnes_profile_begin_frame(); now += 12000;
   hcsnes_profile_end_frame(false,4,2,false,0,false);
   hcsnes_profile_flush("test");
   assert(strstr(line,"core_drawn=0/0 core_skipped=12/12"));
   /* Unequal populations: each average uses its own frame count. */
   for (i=0; i<3; ++i) {
      hcsnes_profile_begin_frame(); now += i == 0 ? 10000 : i == 1 ? 30000 : 90000;
      hcsnes_profile_end_frame(i < 2,4,2,false,0,false);
   }
   hcsnes_profile_flush("test");
   assert(strstr(line,"rendered=2 skipped=1"));
   assert(strstr(line,"core=43/90"));
   assert(strstr(line,"core_drawn=20/30 core_skipped=90/90"));
   hcsnes_profile_begin_frame();
   now = 0; /* Timer failure cannot become an unsigned huge duration. */
   HCSNES_PROFILE_ENTER(HCP_CPU);
   hcsnes_profile_end_frame(true,0,1,false,0,false);
   assert(!hcsnes_profile_enabled);
   puts("PASS: profiler exclusive nesting, frontend separation, cadence, maxima, disabled path, clock failure");
   return 0;
}
