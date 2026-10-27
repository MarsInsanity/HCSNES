/* HCSNES additions, 2026-10-25. See LICENSE-HCSNES. */
#ifndef HCSNES_FRAMESKIP_H
#define HCSNES_FRAMESKIP_H
#include <stdbool.h>
#include <stdint.h>

typedef struct {
   unsigned consecutive;
   bool recovering;
} hcsnes_skip_state;

enum { HCSNES_SKIP_TIMED = 4 };

typedef struct {
   uint64_t next, last, period;
} hcsnes_timed_state;

static unsigned hcsnes_skip_cap(unsigned mode, unsigned limit)
{
   unsigned maximum = mode == HCSNES_SKIP_TIMED ? 3 : 5;
   return limit < maximum ? limit : maximum;
}

static bool hcsnes_limit_skip(hcsnes_skip_state *s, bool requested, unsigned limit)
{
   if (!requested || !limit || s->consecutive >= limit)
   {
      s->consecutive = 0;
      return false;
   }
   ++s->consecutive;
   return true;
}

/* Advance an absolute schedule once per emulated frame. A pause, backwards
 * clock, rate change or fast-forward lead starts a fresh schedule. This state
 * is host-only and must be cleared on reset, load and option changes.
 * The caller supplies the current time and 1/fps in nanoseconds. */
static bool hcsnes_skip_timed_frame(hcsnes_skip_state *s, hcsnes_timed_state *t,
      unsigned limit, uint64_t now, uint64_t period)
{
   bool late;
   s->recovering = false;
   if (!now || !period || now > UINT64_MAX - period)
   {
      t->next = t->last = t->period = 0;
      s->consecutive = 0;
      return false;
   }
   if (!t->last || t->period != period || now < t->last ||
       now - t->last > 100000000ULL ||
       (t->next > now && t->next - now > period) ||
       t->next > UINT64_MAX - period)
   {
      t->next = now;
      s->consecutive = 0;
   }
   late = now > t->next && now - t->next >= period;
   t->last = now;
   t->period = period;
   t->next += period;
   return hcsnes_limit_skip(s, late, hcsnes_skip_cap(HCSNES_SKIP_TIMED, limit));
}

/* Only rendering is skipped. CPU, GSU, SA-1 and audio still advance each call.
 * Auto/threshold modes need frontend feedback; fixed mode deliberately does not.
 * A hard cap guarantees a visible frame even during continuous underrun. */
static bool hcsnes_skip_frame(hcsnes_skip_state *s, unsigned mode,
      unsigned limit, unsigned threshold, bool active,
      unsigned occupancy, bool underrun)
{
   bool requested = false;
   limit = hcsnes_skip_cap(mode, limit);
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
   return hcsnes_limit_skip(s, requested, limit);
}
#endif
