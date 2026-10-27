/* HCSNES standalone additions: see LICENSE-HCSNES. */
#include "hcsnes_profile.h"
#include "hcsnes_version.h"
#include "hcsnes_clock.h"
#include <stdio.h>
#include <string.h>

#define HCP_WINDOW 300
#define HCP_STACK 16
#define HCP_WORST 10
struct hcp_spike {
   uint64_t sequence, total, core, thread_cpu;
   uint64_t section[HCP_SECTION_COUNT], gsu_inst, plot, rpix;
   unsigned skip_mode, skip_cap;
   bool rendered, cpu_valid;
};
bool hcsnes_profile_enabled = false;
static retro_log_printf_t hcp_log;
static struct {
   bool active, invalid;
   unsigned depth, session, frames, rendered, feedback_frames, underruns, worst_count;
   unsigned skip_mode, skip_cap, occupancy;
   uint64_t tick, started, sequence, cpu_started;
   uint64_t frame_gsu_inst, frame_plot, frame_rpix;
   struct hcp_spike worst[HCP_WORST];
   enum hcsnes_profile_section stack[HCP_STACK];
   uint64_t frame[HCP_SECTION_COUNT], sum[HCP_SECTION_COUNT], max[HCP_SECTION_COUNT];
   uint64_t core_sum, core_max, gsu_inst, plot, rpix;
   uint64_t core_drawn_sum, core_drawn_max, core_skipped_sum, core_skipped_max;
} hcp;

#ifdef HCSNES_PROFILE_TEST
extern uint64_t hcsnes_profile_test_clock(void);
extern uint64_t hcsnes_profile_test_cpu_clock(void);
#define hcp_now hcsnes_profile_test_clock
#define hcp_cpu_now hcsnes_profile_test_cpu_clock
#else
#define hcp_now hcsnes_clock_now_ns
#define hcp_cpu_now hcsnes_clock_thread_cpu_ns
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
   hcp.frame_gsu_inst = hcp.frame_plot = hcp.frame_rpix = 0;
   hcp.active = true;
   hcp.invalid = false;
   hcp.depth = 1;
   hcp.stack[0] = HCP_OTHER;
   hcp.tick = hcp_now();
   hcp.cpu_started = hcp_cpu_now();
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
   hcp.frame_gsu_inst += instructions;
   hcp.frame_plot += plot;
   hcp.frame_rpix += rpix;
}

/* Keep the ten largest measured frame totals; ties keep the earlier frame.
 * This is bounded host-only storage, with no allocation or per-frame logging. */
static void hcp_record_spike(uint64_t core, uint64_t total, uint64_t cpu_end,
      bool rendered, unsigned skip_mode, unsigned skip_cap)
{
   struct hcp_spike *s;
   unsigned pos = 0, i, last;
   while (pos < hcp.worst_count && hcp.worst[pos].total >= total) ++pos;
   if (pos == HCP_WORST) return;
   last = hcp.worst_count < HCP_WORST ? hcp.worst_count++ : HCP_WORST - 1;
   for (i = last; i > pos; --i) hcp.worst[i] = hcp.worst[i - 1];
   s = &hcp.worst[pos];
   s->sequence = hcp.sequence;
   s->total = total;
   s->core = core;
   s->rendered = rendered;
   s->skip_mode = skip_mode;
   s->skip_cap = skip_cap;
   s->cpu_valid = hcp.cpu_started != UINT64_MAX && cpu_end != UINT64_MAX &&
      cpu_end >= hcp.cpu_started;
   s->thread_cpu = s->cpu_valid ? cpu_end - hcp.cpu_started : 0;
   memcpy(s->section, hcp.frame, sizeof(s->section));
   s->gsu_inst = hcp.frame_gsu_inst;
   s->plot = hcp.frame_plot;
   s->rpix = hcp.frame_rpix;
}

static void hcp_log_spikes(void)
{
   unsigned i;
   char line[1536];
   for (i = 0; i < hcp.worst_count; ++i) {
      const struct hcp_spike *s = &hcp.worst[i];
      uint64_t off_cpu = s->cpu_valid && s->total > s->thread_cpu ?
         s->total - s->thread_cpu : 0;
#define US(value) (unsigned long long)((value) / 1000)
      snprintf(line, sizeof(line),
         "HCSNES spike v=%s apu=%s fx=%s session=%u window_end=%llu rank=%u "
         "frame=%llu rendered=%u skip_mode=%u cap=%u "
         "total_us=%llu core_us=%llu cpu_other_us=%llu gsu_us=%llu "
         "ppu_us=%llu apu_us=%llu other_us=%llu "
         "video_cb_us=%llu audio_cb_us=%llu input_cb_us=%llu "
         "cpu_clock=%u thread_cpu_us=%llu offcpu_us=%llu "
         "gsu_inst=%llu plot=%llu rpix=%llu\n",
         HCSNES_BUILD_VERSION, HCSNES_APU_NAME, HCSNES_FX_NAME,
         hcp.session, (unsigned long long)hcp.sequence, i + 1,
         (unsigned long long)s->sequence, (unsigned)s->rendered,
         s->skip_mode, s->skip_cap, US(s->total), US(s->core),
         US(s->section[HCP_CPU]), US(s->section[HCP_GSU]),
         US(s->section[HCP_PPU]), US(s->section[HCP_APU]),
         US(s->section[HCP_OTHER]), US(s->section[HCP_VIDEO]),
         US(s->section[HCP_AUDIO]), US(s->section[HCP_INPUT]),
         (unsigned)s->cpu_valid, US(s->thread_cpu), US(off_cpu),
         (unsigned long long)s->gsu_inst, (unsigned long long)s->plot,
         (unsigned long long)s->rpix);
#undef US
      hcp_log(RETRO_LOG_INFO, "%s", line);
   }
}

void hcsnes_profile_flush(const char *reason)
{
   char line[1536];
   uint64_t elapsed, now;
   unsigned n = hcp.frames;
   unsigned skipped = n - hcp.rendered;
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
      "core_drawn=%llu/%llu core_skipped=%llu/%llu "
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
      (unsigned long long)(hcp.rendered ? hcp.core_drawn_sum / hcp.rendered / 1000 : 0),
      (unsigned long long)(hcp.core_drawn_max / 1000),
      (unsigned long long)(skipped ? hcp.core_skipped_sum / skipped / 1000 : 0),
      (unsigned long long)(hcp.core_skipped_max / 1000),
      (unsigned long long)hcp.gsu_inst, (unsigned long long)hcp.plot,
      (unsigned long long)hcp.rpix);
#undef AVG
#undef MAX
   hcp_log(RETRO_LOG_INFO, "%s", line);
   hcp_log_spikes();
   hcp.worst_count = 0;
   memset(hcp.sum, 0, sizeof(hcp.sum));
   memset(hcp.max, 0, sizeof(hcp.max));
   hcp.frames = hcp.rendered = hcp.feedback_frames = hcp.underruns = 0;
   hcp.core_sum = hcp.core_max = hcp.gsu_inst = hcp.plot = hcp.rpix = 0;
   hcp.core_drawn_sum = hcp.core_drawn_max = 0;
   hcp.core_skipped_sum = hcp.core_skipped_max = 0;
}

void hcsnes_profile_end_frame(bool rendered, unsigned skip_mode, unsigned skip_cap,
      bool feedback, unsigned occupancy, bool underrun)
{
   unsigned i;
   uint64_t core = 0, total = 0, cpu_end;
   if (!hcp.active) return;
   cpu_end = hcp_cpu_now();
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
      total += time;
   }
   hcp.core_sum += core;
   if (core > hcp.core_max) hcp.core_max = core;
   if (rendered) {
      hcp.core_drawn_sum += core;
      if (core > hcp.core_drawn_max) hcp.core_drawn_max = core;
   } else {
      hcp.core_skipped_sum += core;
      if (core > hcp.core_skipped_max) hcp.core_skipped_max = core;
   }
   ++hcp.frames;
   ++hcp.sequence;
   hcp_record_spike(core, total, cpu_end, rendered, skip_mode, skip_cap);
   hcp.rendered += rendered ? 1 : 0;
   hcp.feedback_frames += feedback ? 1 : 0;
   hcp.underruns += feedback && underrun ? 1 : 0;
   hcp.skip_mode = skip_mode;
   hcp.skip_cap = skip_cap;
   hcp.occupancy = occupancy;
   if (hcp.frames == HCP_WINDOW) hcsnes_profile_flush("window");
}
