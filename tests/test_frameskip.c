/* HCSNES regression tests. See LICENSE-HCSNES. */
#include <assert.h>
#include <stdio.h>
#include "../source/hcsnes_frameskip.h"
static void test_timed(void)
{
   hcsnes_skip_state s = {0, false};
   hcsnes_timed_state t = {0, 0, 0};
   uint64_t now, period;
   unsigned cap, i, rate, run, shown;
   const uint64_t periods[] = {16639267, 20000000};
   /* Both rates, with deliberate sustained lateness and every exposed cap. */
   for (rate = 0; rate < 2; ++rate)
   {
      period = periods[rate];
      for (cap = 0; cap <= 5; ++cap)
      {
         t.last = 0; s.consecutive = 0; now = 1000000000; run = shown = 0;
         for (i = 0; i < 600; ++i, now += 2 * period)
         {
            bool skip = hcsnes_skip_timed_frame(&s, &t, cap, now, period);
            if (skip) ++run; else { ++shown; run = 0; }
            assert(run <= hcsnes_skip_cap(HCSNES_SKIP_TIMED, cap));
         }
         assert(shown == (cap ? 600 / (hcsnes_skip_cap(HCSNES_SKIP_TIMED, cap) + 1) : 600));
      }
      /* Exact and slightly early pacing never skip. */
      t.last = 0; now = 1000000000;
      for (i = 0; i < 1000; ++i, now += period)
         assert(!hcsnes_skip_timed_frame(&s, &t, 2, now, period));
      for (i = 0; i < 1000; ++i, now += period - 1000)
         assert(!hcsnes_skip_timed_frame(&s, &t, 2, now, period));
   }
   period = 20000000;
   t.last = 0; now = 1000000000;
   assert(!hcsnes_skip_timed_frame(&s, &t, 2, now, period));
   now += period + period / 2;
   assert(!hcsnes_skip_timed_frame(&s, &t, 2, now, period)); /* strict > half */
   now += period + 1;
   assert(hcsnes_skip_timed_frame(&s, &t, 2, now, period));
   now = t.next; /* Catch-up returns to drawing immediately. */
   assert(!hcsnes_skip_timed_frame(&s, &t, 2, now, period));
   now += 100000000;
   assert(hcsnes_skip_timed_frame(&s, &t, 2, now, period));
   now += 100000001; /* Menu/pause drops old debt and starts by drawing. */
   assert(!hcsnes_skip_timed_frame(&s, &t, 2, now, period));
   assert(s.consecutive == 0 && t.next == now + period);
   assert(!hcsnes_skip_timed_frame(&s, &t, 2, now - 1, period));
   assert(!hcsnes_skip_timed_frame(&s, &t, 2, 0, period));
   assert(!t.last && !s.consecutive);
   assert(!hcsnes_skip_timed_frame(&s, &t, 2, now, 0));
   assert(!hcsnes_skip_timed_frame(&s, &t, 2, UINT64_MAX, period));
   assert(!hcsnes_skip_timed_frame(&s, &t, 2, now, period));
   assert(!hcsnes_skip_timed_frame(&s, &t, 2, now + 2 * period, period + 1));
   /* Unpaced/fast-forward calls cannot accumulate an unbounded future schedule. */
   t.last = 0;
   for (i = 0; i < 10000; ++i, now += 1000)
   {
      assert(!hcsnes_skip_timed_frame(&s, &t, 2, now, period));
      assert(t.next - now <= 2 * period);
   }
   assert(hcsnes_skip_timed_frame(&s, &t, 2, t.next + period, period));
}

int main(void)
{
   hcsnes_skip_state s = {0, false};
   unsigned mode, cap, i, run, shown;
   for (mode=0; mode<4; ++mode)
      for (cap=0; cap<=5; ++cap)
      {
         s.consecutive=0; s.recovering=false; run=shown=0;
         for (i=0; i<600; ++i)
         {
            bool skip=hcsnes_skip_frame(&s,mode,cap,33,true,0,true);
            if (skip) ++run; else {++shown; run=0;}
            assert(run<=cap);
         }
         assert(shown>=600/(cap+1));
         if (!mode || !cap) assert(shown==600);
      }
   s.consecutive=0; s.recovering=false;
   assert(!hcsnes_skip_frame(&s,1,1,33,false,0,true));
   assert(!hcsnes_skip_frame(&s,2,1,33,false,0,true));
   assert(hcsnes_skip_frame(&s,3,1,33,false,100,false));
   assert(!hcsnes_skip_frame(&s,3,1,33,false,100,false));
   s.consecutive=0; s.recovering=false;
   assert(hcsnes_skip_frame(&s,2,5,33,true,32,false));
   assert(hcsnes_skip_frame(&s,2,5,33,true,38,false));
   assert(!hcsnes_skip_frame(&s,2,5,33,true,43,false));
   assert(!hcsnes_skip_frame(&s,2,5,33,true,38,false));
   s.consecutive=0; s.recovering=false;
   for(i=0;i<100;++i) {
      bool skip=hcsnes_skip_frame(&s,1,99,1000,true,1000,true);
      assert(s.consecutive<=5);
      (void)skip;
   }
   test_timed();
   puts("PASS: frameskip caps, fixed cadence, hysteresis, timed deadlines/rates, pause, clock failure and fast-forward");
   return 0;
}
