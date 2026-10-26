# HCSNES validation — 2026-10-25

## 0.2.0 validation

The native GCC run passed the 33,536-case Super FX suite with profiling both
Off and On (67,072 comparisons), deterministic timing tests, and the Fast/Plus
full-core tests. Profiling produced the expected two 300-frame log windows
without changing WRAM replay or video output. Unit checks cover nested time
accounting, maxima, callback exclusion, missing log support and clock failure.

CI now repeats native validation at O2 and O3, verifies the GCC/sysroot build
interface, cross-builds all three MIPS cores and the diagnostics, checks ELF
ABI/API exports, and produces the complete SD-card package. Consult the Actions
run for the exact revision's current result. Native previous-version state tests also passed for both Fast (562192 bytes)
and Plus (564544 bytes) on Linux x86-64: the 0.1 state loaded into 0.2 and
replayed to the same WRAM contents. These are host sizes, not device sizes.
On-device tests remain outstanding.
The existing raw files below record the original 0.1 validation, not a new
handheld benchmark.

## Original 0.1 executed checks

- Native Windows x86-64 C builds with Zig 0.13.0 / Clang 18.1.6.
- 33,536 Super FX differential cases passed: all 1,024 ALT/opcode entries,
  four plot modes, eight register/flag setups, 0/64-instruction budgets,
  halted state, ROM-buffer/PC boundary cases and memory/register comparison.
- Frame-skip tests passed: maximum run length, fixed cadence, missing feedback,
  threshold recovery hysteresis, and out-of-range limits.
- RTC encoding tests passed: fixed eight-byte little-endian format, round trips,
  32-bit boundary values, and adjacent-buffer guard bytes.
- Both Fast and Plus passed real libretro ABI tests with an original test ROM:
  ROM load, 120 frames of RGB565 video/audio, nonzero backdrop output,
  save/load deterministic WRAM replay, option switching, fixed 1:1 rendered/
  duplicated cadence, audio during skipped frames, and no automatic skipping
  when frontend audio-buffer feedback is unavailable.
- Fast, Plus and Fast-reference cross-compiled and linked for the handheld.
- The benchmark and Super FX test executable cross-compiled and linked.
- All five device binaries passed ELF inspection: little-endian MIPS32r2,
  O32, hard-float ABI with 32-bit FPU registers.
- Generated dispatch map checked against the authoritative 1,024-entry table.

Raw output is in validation/native-tests.txt and validation/mips-abi.txt.
The source includes the test cases and rerunnable test driver.

## Limits

The differential tests compare the inherited and modified Super FX interpreters;
they are not independent chip-accuracy tests. The full-core runtime test uses an
original simple LoROM with no enhancement chip, not a commercial game.

The original 0.1 cores import libc.so.6; observed version strings reach GLIBC_2.7.
The 0.2 profiler adds clock_gettime; the supplied Zig target uses a glibc 2.17
baseline. Check the new ELF report and the firmware SDK/runtime for compatibility.
The diagnostic benchmark additionally imports libdl.so.2 and GLIBC_2.17.
ELF matching does not establish exact handheld loader/runtime compatibility.
The MIPS diagnostic executables are compiled but have not run on MIPS hardware.

No R36S/SF3000 handheld was attached. Yoshi's Island, Star Fox, demanding SA-1
titles, SDL presentation, controller mappings, actual audio and device speed
remain unmeasured. No FPS improvement, universal full-speed result, or broad
game-compatibility claim is made.

The upstream project supplies mature emulation. This work adds a focused
dispatch path, frame-skip changes, a 32-bit RTC fix, builds and diagnostics.
