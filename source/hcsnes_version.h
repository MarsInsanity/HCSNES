/* HCSNES standalone additions: see LICENSE-HCSNES. */
#ifndef HCSNES_VERSION_H
#define HCSNES_VERSION_H
#define HCSNES_VERSION "0.4.0"
#ifndef HCSNES_GIT_HASH
#define HCSNES_GIT_HASH "unknown"
#endif
#define HCSNES_BUILD_VERSION HCSNES_VERSION "+" HCSNES_GIT_HASH
#ifdef USE_BLARGG_APU
#define HCSNES_APU_NAME "plus"
#else
#define HCSNES_APU_NAME "fast"
#endif
#ifdef HCSNES_THREADED_FX
#define HCSNES_FX_NAME "threaded"
#else
#define HCSNES_FX_NAME "reference"
#endif
#endif
