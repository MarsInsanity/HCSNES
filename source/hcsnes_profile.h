/* HCSNES standalone additions: see LICENSE-HCSNES.
 * Host-only diagnostics. Never add this state to an emulated/saved structure. */
#ifndef HCSNES_PROFILE_H
#define HCSNES_PROFILE_H
#include <stdbool.h>
#include <stdint.h>
#include <libretro.h>

enum hcsnes_profile_section {
   HCP_OTHER, HCP_CPU, HCP_GSU, HCP_PPU, HCP_APU,
   HCP_VIDEO, HCP_AUDIO, HCP_INPUT, HCP_SECTION_COUNT
};
extern bool hcsnes_profile_enabled;
void hcsnes_profile_configure(bool enabled, retro_log_printf_t logger);
void hcsnes_profile_reset(void);
void hcsnes_profile_flush(const char *reason);
void hcsnes_profile_begin_frame(void);
void hcsnes_profile_end_frame(bool rendered, unsigned skip_mode, unsigned skip_cap,
      bool feedback, unsigned occupancy, bool underrun);
void hcsnes_profile_enter(enum hcsnes_profile_section section);
void hcsnes_profile_leave(void);
void hcsnes_profile_gsu_counts(uint64_t instructions, uint64_t plot, uint64_t rpix);

/* No clock reads or function calls on the disabled path. */
#define HCSNES_PROFILE_ENTER(section) do { \
   if (hcsnes_profile_enabled) hcsnes_profile_enter(section); \
} while (0)
#define HCSNES_PROFILE_LEAVE() do { \
   if (hcsnes_profile_enabled) hcsnes_profile_leave(); \
} while (0)
#endif
