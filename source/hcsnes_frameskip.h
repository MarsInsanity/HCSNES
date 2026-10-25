/* HCSNES additions, 2026-10-25. See LICENSE-HCSNES. */
#ifndef HCSNES_FRAMESKIP_H
#define HCSNES_FRAMESKIP_H
#include <stdbool.h>

typedef struct {
   unsigned consecutive;
   bool recovering;
} hcsnes_skip_state;

/* Only rendering is skipped. CPU, GSU, SA-1 and audio still advance each call.
 * Auto/threshold modes need frontend feedback; fixed mode deliberately does not.
 * A hard cap guarantees a visible frame even during continuous underrun. */
static bool hcsnes_skip_frame(hcsnes_skip_state *s, unsigned mode,
      unsigned limit, unsigned threshold, bool active,
      unsigned occupancy, bool underrun)
{
   bool requested = false;
   if (limit > 5) limit = 5;
   if (threshold > 100) threshold = 100;
   if (occupancy > 100) occupancy = 100;
   if (mode == 3)
      requested = true;
   else if (active && mode == 1)
      requested = underrun;
   else if (active && mode == 2)
   {
      unsigned recovery = threshold > 90 ? 100 : threshold + 10;
      if (occupancy < threshold) s->recovering = true;
      else if (occupancy >= recovery) s->recovering = false;
      requested = s->recovering || underrun;
   }
   else s->recovering = false;
   if (!requested || !limit || s->consecutive >= limit)
   {
      s->consecutive = 0;
      return false;
   }
   ++s->consecutive;
   return true;
}
#endif
