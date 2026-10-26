/* HCSNES standalone additions: see LICENSE-HCSNES. */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include "hcsnes_profile.h"
#include "hcsnes_version.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <time.h>
#endif

#define HCP_WINDOW 300
#define HCP_STACK 16
bool hcsnes_profile_enabled = false;
static retro_log_printf_t hcp_log;
static struct {
   bool active, invalid;
   unsigned depth, session, frames, rendered, feedback_frames, underruns;
   unsigned skip_mode, skip_cap, occupancy;
   uint64_t tick, started, sequence;
   enum hcsnes_profile_section stack[HCP_STACK];
   uint64_t frame[HCP_SECTION_COUNT], sum[HCP_SECTION_COUNT], max[HCP_SECTION_COUNT];
   uint64_t core_sum, core_max, gsu_inst, plot, rpix;
} hcp;

#ifdef HCSNES_PROFILE_TEST
extern uint64_t hcsnes_profile_test_clock(void);
#define hcp_now hcsnes_profile_test_clock
#else
static uint64_t hcp_now(void)
{
#ifdef _WIN32
   LARGE_INTEGER counter, frequency;
   if (!QueryPerformanceCounter(&counter) || !QueryPerformanceFrequency(&frequency))
      return 0;
   return (uint64_t)(counter.QuadPart / frequency.QuadPart) * 1000000000ULL +
      (uint64_t)(counter.QuadPart % frequency.QuadPart) * 1000000000ULL /
      (uint64_t)frequency.QuadPart;
#else
   struct timespec now;
   if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
      return 0;
   return (uint64_t)now.tv_sec * 1000000000ULL + (uint64_t)now.tv_nsec;
#endif
}
#endif

static void hcp_charge(void)
{
   uint64_t now = hcp_now();
   if (!now || now < hcp.tick) {
      hcp.invalid = true;
      return;
   }
   hcp.frame[hcp.stack[hcp.depth - 1]] += now - hcp.tick;
   hcp.tick = now;
}

void hcsnes_profile_reset(void)
{
   unsigned session = hcp.session + 1;
   memset(&hcp, 0, sizeof(hcp));
   hcp.session = session;
   if (hcsnes_profile_enabled)
      hcp.started = hcp_now();
}

void hcsnes_profile_configure(bool enabled, retro_log_printf_t logger)
{
   bool requested = enabled && logger != NULL;
   if (hcsnes_profile_enabled != requested) {
      if (hcsnes_profile_enabled)
         hcsnes_profile_flush("off");
      hcp_log = logger;
      hcsnes_profile_enabled = requested;
      hcsnes_profile_reset();
   }
   hcp_log = logger;
}

void hcsnes_profile_begin_frame(void)
{
   if (!hcsnes_profile_enabled) return;
   memset(hcp.frame, 0, sizeof(hcp.frame));
   hcp.active = true;
   hcp.invalid = false;
   hcp.depth = 1;
   hcp.stack[0] = HCP_OTHER;
   hcp.tick = hcp_now();
   if (!hcp.tick) hcp.invalid = true;
}

void hcsnes_profile_enter(enum hcsnes_profile_section section)
{
   if (!hcp.active || hcp.invalid) return;
   if (hcp.depth >= HCP_STACK || section >= HCP_SECTION_COUNT) {
      hcp.invalid = true;
      return;
   }
   hcp_charge();
   hcp.stack[hcp.depth++] = section;
}

void hcsnes_profile_leave(void)
{
   if (!hcp.active || hcp.invalid) return;
   if (hcp.depth <= 1) { hcp.invalid = true; return; }
   hcp_charge();
   --hcp.depth;
}

void hcsnes_profile_gsu_counts(uint64_t instructions, uint64_t plot, uint64_t rpix)
{
   if (!hcp.active || hcp.invalid) return;
   hcp.gsu_inst += instructions;
   hcp.plot += plot;
   hcp.rpix += rpix;
}

void hcsnes_profile_flush(const char *reason)
{
   char line[1536];
   uint64_t elapsed, now;
   unsigned n = hcp.frames;
   if (!n || !hcp_log) return;
   now = hcp_now();
   elapsed = now >= hcp.started ? (now - hcp.started) / 1000000ULL : 0;
#define AVG(s) (unsigned long long)(hcp.sum[s] / n / 1000)
#define MAX(s) (unsigned long long)(hcp.max[s] / 1000)
   snprintf(line, sizeof(line),
      "HCSNES profile v=%s apu=%s fx=%s session=%u frame=%llu n=%u reason=%s "
      "elapsed_ms=%llu rendered=%u skipped=%u skip_mode=%u cap=%u "
      "feedback=%u underruns=%u occupancy=%u "
      "us_avg/max core=%llu/%llu cpu_other=%llu/%llu gsu=%llu/%llu "
      "ppu=%llu/%llu apu=%llu/%llu other=%llu/%llu "
      "video_cb=%llu/%llu audio_cb=%llu/%llu input_cb=%llu/%llu "
      "gsu_inst=%llu plot=%llu rpix=%llu\n",
      HCSNES_BUILD_VERSION, HCSNES_APU_NAME, HCSNES_FX_NAME,
      hcp.session, (unsigned long long)hcp.sequence, n, reason,
      (unsigned long long)elapsed, hcp.rendered, n - hcp.rendered,
      hcp.skip_mode, hcp.skip_cap, hcp.feedback_frames, hcp.underruns, hcp.occupancy,
      (unsigned long long)(hcp.core_sum / n / 1000),
      (unsigned long long)(hcp.core_max / 1000),
      AVG(HCP_CPU), MAX(HCP_CPU), AVG(HCP_GSU), MAX(HCP_GSU),
      AVG(HCP_PPU), MAX(HCP_PPU), AVG(HCP_APU), MAX(HCP_APU),
      AVG(HCP_OTHER), MAX(HCP_OTHER), AVG(HCP_VIDEO), MAX(HCP_VIDEO),
      AVG(HCP_AUDIO), MAX(HCP_AUDIO), AVG(HCP_INPUT), MAX(HCP_INPUT),
      (unsigned long long)hcp.gsu_inst, (unsigned long long)hcp.plot,
      (unsigned long long)hcp.rpix);
#undef AVG
#undef MAX
   hcp_log(RETRO_LOG_INFO, "%s", line);
   memset(hcp.sum, 0, sizeof(hcp.sum));
   memset(hcp.max, 0, sizeof(hcp.max));
   hcp.frames = hcp.rendered = hcp.feedback_frames = hcp.underruns = 0;
   hcp.core_sum = hcp.core_max = hcp.gsu_inst = hcp.plot = hcp.rpix = 0;
}

void hcsnes_profile_end_frame(bool rendered, unsigned skip_mode, unsigned skip_cap,
      bool feedback, unsigned occupancy, bool underrun)
{
   unsigned i;
   uint64_t core = 0;
   if (!hcp.active) return;
   if (!hcp.invalid && hcp.depth == 1) hcp_charge();
   else hcp.invalid = true;
   hcp.active = false;
   if (hcp.invalid) {
      /* Never publish timings with a failed clock or unbalanced scopes. */
      if (hcp_log) hcp_log(RETRO_LOG_WARN,
         "HCSNES profiling disabled: clock failure or unbalanced scopes.\n");
      hcsnes_profile_enabled = false;
      hcsnes_profile_reset();
      return;
   }
   for (i = 0; i < HCP_SECTION_COUNT; ++i) {
      uint64_t time = hcp.frame[i];
      hcp.sum[i] += time;
      if (time > hcp.max[i]) hcp.max[i] = time;
      if (i <= HCP_APU) core += time;
   }
   hcp.core_sum += core;
   if (core > hcp.core_max) hcp.core_max = core;
   ++hcp.frames;
   ++hcp.sequence;
   hcp.rendered += rendered ? 1 : 0;
   hcp.feedback_frames += feedback ? 1 : 0;
   hcp.underruns += feedback && underrun ? 1 : 0;
   hcp.skip_mode = skip_mode;
   hcp.skip_cap = skip_cap;
   hcp.occupancy = occupancy;
   if (hcp.frames == HCP_WINDOW) hcsnes_profile_flush("window");
}
