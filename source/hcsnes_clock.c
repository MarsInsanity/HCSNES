/* HCSNES standalone additions: see LICENSE-HCSNES. */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include "hcsnes_clock.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <time.h>
#endif

uint64_t hcsnes_clock_now_ns(void)
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

/* Optional profiling diagnostic: excludes time when this thread is not running.
 * It never drives emulation or frameskip. */
uint64_t hcsnes_clock_thread_cpu_ns(void)
{
#ifdef _WIN32
   FILETIME created, exited, kernel, user;
   uint64_t kernel_ticks, user_ticks;
   if (!GetThreadTimes(GetCurrentThread(), &created, &exited, &kernel, &user))
      return UINT64_MAX;
   kernel_ticks = ((uint64_t)kernel.dwHighDateTime << 32) | kernel.dwLowDateTime;
   user_ticks = ((uint64_t)user.dwHighDateTime << 32) | user.dwLowDateTime;
   return (kernel_ticks + user_ticks) * 100;
#elif defined(CLOCK_THREAD_CPUTIME_ID)
   struct timespec now;
   if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &now) != 0)
      return UINT64_MAX;
   return (uint64_t)now.tv_sec * 1000000000ULL + (uint64_t)now.tv_nsec;
#else
   return UINT64_MAX;
#endif
}
