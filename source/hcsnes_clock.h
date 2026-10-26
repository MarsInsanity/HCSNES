/* Host-only clock shared by profiling and timed frameskip. See LICENSE-HCSNES. */
#ifndef HCSNES_CLOCK_H
#define HCSNES_CLOCK_H
#include <stdint.h>
/* Nanoseconds from an arbitrary monotonic origin; zero means clock failure. */
uint64_t hcsnes_clock_now_ns(void);
#endif
