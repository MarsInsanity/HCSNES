/* Deterministic accounting tests: nested work, maxima, callbacks, cadence. */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "../source/hcsnes_profile.h"
static uint64_t now = 1;
static unsigned reports, reads, cpu_reads, spike_count;
static bool cpu_failure;
static char spikes[10][2048];
static char line[2048];
uint64_t hcsnes_profile_test_clock(void) { ++reads; return now; }
uint64_t hcsnes_profile_test_cpu_clock(void) {
   ++cpu_reads; return cpu_failure ? UINT64_MAX : now / 2;
}
static void logger(enum retro_log_level level, const char *fmt, ...)
{
   char entry[2048];
   va_list ap; (void)level;
   va_start(ap, fmt); vsnprintf(entry, sizeof(entry), fmt, ap); va_end(ap);
   if (strstr(entry, "HCSNES profile ")) {
      ++reports; spike_count = 0;
      memcpy(line, entry, sizeof(line));
   } else if (strstr(entry, "HCSNES spike ")) {
      assert(spike_count < 10);
      memcpy(spikes[spike_count++], entry, sizeof(entry));
   }
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
   assert(reads == 0 && cpu_reads == 0 && reports == 0 && spike_count == 0);
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
   assert(spike_count == 10 && cpu_reads == 600);
   for (i=0; i<10; ++i) {
      char identity[80];
      snprintf(identity, sizeof(identity), "rank=%u frame=%u rendered=1", i+1, i*2+1);
      assert(strstr(spikes[i], identity)); /* stable ties, earliest ten */
      assert(strstr(spikes[i], "window_end=300"));
      assert(strstr(spikes[i], "total_us=43 core_us=22 cpu_other_us=6 gsu_us=3"));
      assert(strstr(spikes[i], "cpu_clock=1 thread_cpu_us=21 offcpu_us=21"));
      assert(strstr(spikes[i], "gsu_inst=7 plot=2 rpix=1"));
   }
   hcsnes_profile_begin_frame();
   now+=8000;
   hcsnes_profile_end_frame(true,0,1,false,0,false);
   hcsnes_profile_configure(false,logger);
   assert(reports==2 && strstr(line,"n=1 reason=off"));
   assert(strstr(line,"core=8/8") && strstr(line,"gsu_inst=0"));
   assert(strstr(line,"core_drawn=8/8 core_skipped=0/0"));
   assert(spike_count == 1 && strstr(spikes[0], "frame=301 rendered=1"));
   assert(strstr(spikes[0], "gsu_inst=0 plot=0 rpix=0"));
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
   /* More than ten distinct frames, including a callback-dominated spike. */
   for (i=0; i<20; ++i) {
      hcsnes_profile_begin_frame();
      now += (i+1)*1000;
      hcsnes_profile_gsu_counts(i+1, i, 0);
      if (i == 0) {
         HCSNES_PROFILE_ENTER(HCP_VIDEO); now += 100000; HCSNES_PROFILE_LEAVE();
      }
      hcsnes_profile_end_frame(i % 2, 4, 3, false, 0, false);
   }
   hcsnes_profile_flush("rank_test");
   assert(spike_count == 10);
   assert(strstr(spikes[0], "total_us=101 core_us=1"));
   assert(strstr(spikes[0], "gsu_inst=1 plot=0 rpix=0"));
   for (i=1; i<10; ++i) {
      char duration[64], count[64];
      snprintf(duration, sizeof(duration), "total_us=%u core_us=%u", 21-i, 21-i);
      snprintf(count, sizeof(count), "gsu_inst=%u plot=%u rpix=0", 21-i, 20-i);
      assert(strstr(spikes[i], duration) && strstr(spikes[i], count));
   }
   cpu_failure = true;
   hcsnes_profile_begin_frame(); now += 10000;
   hcsnes_profile_end_frame(false,4,3,false,0,false);
   hcsnes_profile_flush("cpu_unavailable");
   assert(hcsnes_profile_enabled && spike_count == 1);
   assert(strstr(spikes[0], "cpu_clock=0 thread_cpu_us=0 offcpu_us=0"));
   cpu_failure = false;
   hcsnes_profile_reset();
   hcsnes_profile_begin_frame(); now += 1000;
   hcsnes_profile_end_frame(true,4,3,false,0,false);
   hcsnes_profile_flush("reset_test");
   assert(spike_count == 1 && strstr(spikes[0], "frame=1 rendered=1"));
   hcsnes_profile_begin_frame();
   now = 0; /* Timer failure cannot become an unsigned huge duration. */
   HCSNES_PROFILE_ENTER(HCP_CPU);
   hcsnes_profile_end_frame(true,0,1,false,0,false);
   assert(!hcsnes_profile_enabled);
   puts("PASS: profiler exclusive nesting, frontend separation, cadence, maxima, disabled path, clock failure");
   return 0;
}
