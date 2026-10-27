/* Real platform clocks: zero CPU time is valid, unavailability is explicit. */
#include <assert.h>
#include <stdio.h>
#include "../source/hcsnes_clock.h"
int main(void)
{
   uint64_t wall = hcsnes_clock_now_ns();
   uint64_t cpu = hcsnes_clock_thread_cpu_ns();
   uint64_t cpu_after = hcsnes_clock_thread_cpu_ns();
   assert(wall && hcsnes_clock_now_ns() >= wall);
#if defined(__linux__) || defined(_WIN32)
   assert(cpu != UINT64_MAX && cpu_after != UINT64_MAX);
#endif
   assert(cpu == UINT64_MAX || cpu_after == UINT64_MAX || cpu_after >= cpu);
   puts("PASS: monotonic wall and thread CPU clocks");
   return 0;
}
