/* HCSNES regression tests. See LICENSE-HCSNES. */
#include <assert.h>
#include <stdio.h>
#include "../source/hcsnes_frameskip.h"
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
   puts("PASS: frameskip cap, fixed cadence, hysteresis and missing feedback");
   return 0;
}
