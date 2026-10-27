# Measuring HCSNES on the updated ShallotsOS build

## Establish a fresh baseline

The latest ShallotsOS device report found background polling that started
roughly 24 processes per second during games. Other processes used about 25%
of the CPU before the fix and about 7–8% afterward. **Earlier HCSNES timings
must not be used as the current performance baseline.** Use the updated
firmware for both profiling and normal-play comparisons.

The user reports a large improvement after that fix, with occasional remaining
lows and spikes. HCSNES 0.4 targets their diagnosis and adjusts Timed to the
newly measured stock rule. It does not claim a measured device speedup.

## No device shell required

1. Install the ShallotsOS build with the background-polling fix. Record its
   version alongside the core version, game region and exact scene.
2. Shut down, insert the SD card into your computer, and back up saves and
   existing HCSNES cores. Copy the package's **sdcard** contents to the card root.
3. Create an empty **log.txt** at the card root. On Windows, show file-name
   extensions so it is not accidentally named `log.txt.txt`.
4. Select **hcsnes_fast**, **Frameskip = Timed**, **Maximum Consecutive Skips = 3**,
   and **Profiling = On**. Leave emulated CPU overclocking and sprite-limit
   removal Off. Disable any separate frontend frameskip controller.
5. Play the same demanding Yoshi's Island scene for **60 seconds** after
   entering gameplay. Avoid changing settings halfway through the sample.
6. Exit normally, shut down, and copy **logs/cores.log** and
   **logs/picoarch.log**. Keep files named by firmware/core/scene/settings.
7. Repeat the scene with **Profiling = Off** to judge actual audio and
   smoothness. Logging and measurement add overhead and may create hitches.
8. For dispatch comparison, repeat with **hcsnes_fast_reference** using the
   same settings. Compare Plus separately when investigating audio behavior;
   Fast and Plus cannot share raw states.

Auto remains the default, with cap 1. To isolate the 0.4 Timed threshold change
from its extra allowed skip, compare 0.3 and 0.4 at cap 2 on the **same fixed
firmware**, then compare 0.4 at cap 3. Keep overlays, filtering, background
activity and the scene constant. Do not compare an old overloaded-system
profile against a new fixed-system profile as if only the core changed.

ShallotsOS forwards `RETRO_LOG_INFO` lines to cores.log when the marker exists.
Other PicoArch builds may use different controls. HCSNES uses the frontend
logger; it does not open or overwrite a device log. With no logger, profiling
stays inactive. The `HCSNES build` line identifies the running binary.

## Window summaries

Each `HCSNES profile` line covers 300 emulated frames. Partial windows flush
when profiling turns Off, a state loads, the game resets, or content unloads.
`session` and `frame` identify the sequence; reset/state load starts a new one.

The `us_avg/max` pairs are average/maximum microseconds per frame. Most use
all frames in the window; `core_drawn` uses only rendered frames and
`core_skipped` uses only skipped frames. An empty group reports `0/0`; check
its count before interpreting it. Nested work is charged exclusively.

| Field | Meaning |
|---|---|
| `core` | All measured core work, excluding frontend input/video/audio callbacks |
| `core_drawn`, `core_skipped` | Core cost for each group's frames only |
| `cpu_other` | Main-loop time after measured GSU, PPU and APU work is removed |
| `gsu` | Super FX execution, including profiling counter overhead |
| `ppu` | RenderLine and S9xUpdateScreen, including sprite setup reached there |
| `apu` | Mixing/output preparation; Plus also includes batched SPC/DSP and port synchronization |
| `other` | Remaining measured core setup/bookkeeping |
| `video_cb`, `audio_cb`, `input_cb` | Frontend callback time on the calling thread |
| `gsu_inst`, `plot`, `rpix` | Exact operation totals for the window |
| `rendered`, `skipped` | Image frames versus duplicate/no-image frames |
| `feedback`, `underruns` | Frames with active audio feedback and reported underrun risk |
| `occupancy` | Last audio-ring occupancy percentage, not a window average |
| `skip_mode`, `cap` | 0=Off, 1=Auto, 2=Threshold, 3=Fixed, 4=Timed; effective skip cap |
| `elapsed_ms` | Wall time since session start, including pauses/menus; not FPS |

Fast's lockstep SPC700 execution remains in `cpu_other`. That field also
includes DMA, SA-1, PPU register writes and other uninstrumented work; it is not
a pure 65816 measurement. Timing every SPC instruction would distort the run.

## Ten worst frames

Each summary is followed by up to ten `HCSNES spike` lines, ranked by measured
frame wall time, largest first. Equal durations keep earlier frames. A partial
window shorter than ten frames reports only the frames it contains. Records
are bounded in memory; sorting and logging do not run at each GSU instruction.

| Field | Meaning |
|---|---|
| `session`, `window_end`, `rank`, `frame` | Window identity, descending rank and exact emulated frame number |
| `rendered`, `skip_mode`, `cap` | This frame's drawing decision and policy |
| `total_us` | Sum of all measured sections, including frontend callbacks |
| `core_us` | This frame's core work, excluding frontend callbacks |
| `cpu_other_us`, `gsu_us`, `ppu_us`, `apu_us`, `other_us` | Exclusive subsystem breakdown for this frame |
| `video_cb_us`, `audio_cb_us`, `input_cb_us` | This frame's callback durations |
| `gsu_inst`, `plot`, `rpix` | This frame's exact counts, not cumulative window totals |
| `cpu_clock` | 1 if calling-thread CPU time was available and valid, otherwise 0 |
| `thread_cpu_us` | CPU time consumed by the calling thread during the measured frame |
| `offcpu_us` | Approximate non-running time: max(total minus thread CPU time, 0) |

The optional CPU clock is read twice per profiled frame. It never controls
frameskip or emulation. Its absence does not disable profiling; when
`cpu_clock=0`, both CPU-related durations are reported as zero and must be
treated as unavailable. Clock resolution and boundary-read overhead mean
`offcpu_us` is an estimate. On coarse timers, CPU time may exceed wall time
for an individual short frame; the difference is clamped to zero.

Interpret the records in this order:

1. Compare GSU instruction counts for slow frames against normal work in the
   same scene. More instructions can explain more work in one host frame.
2. Check `offcpu_us` and callback durations. A large non-running component
   can indicate scheduling or blocking, not expensive GSU instructions.
3. If instruction counts stay similar while GSU time grows and off-CPU time
   stays small, investigate CPU-side execution. Timing alone cannot identify
   cache/TLB misses or prove a specific hardware cause.
4. Compare `core_skipped` with the stock baseline. If skipped frames remain
   expensive on the fixed system, optimize the measured subsystem before
   attempting a specialized renderer.

The thread CPU measurement covers the whole measured frame, including
callbacks and profiler work; it cannot assign preemption to one subsystem.
Timing starts after option processing and ends before formatting/logging the
window. It excludes frontend work after retro_run returns. A long spike log
write can delay the next frame; Profiling Off is required for clean play tests.

## Source audit: skipped frames and long GSU calls

- In `source/gfx.c`, a skipped `RenderLine` bypasses background/matrix line
  decoding. It still calls `S9xSetupOBJ` when `OBJChanged` and updates the
  sprite range/time-over flags. Those flags are observable emulated state.
- Drawn frames advance `IPPU.CurrentLine`; `S9xUpdateScreen` brings
  `IPPU.PreviousLine` up to it. Skipped lines do not advance CurrentLine.
  `FLUSH_REDRAW` in `source/ppu.h` only updates the screen when these differ.
  Normal skipped frames therefore avoid RenderScreen, clip-window rebuilding
  and pixel colour math. Required PPU register/memory writes still execute.
- `S9xSuperFXExec` uses `FxEmulate(~0)` on the normal Yoshi's Island path, so
  long programs can run to STOP within one host frame. The inherited
  instruction budget, interrupts and synchronization are unchanged. Splitting
  this work across host frames without preserving SNES timing is not a safe
  optimization.
- `Settings.Shutdown` is already enabled by default; Yoshi's Island is not
  on the inherited compatibility exception list. The startup log confirms
  the actual setting.

This release does not remove required sprite flags or introduce a speculative
Mode 1/colour-math fast path. Fresh spike and skipped-frame measurements guide
the next optimization; visual/state equivalence tests must accompany renderer
changes. PGO collection should likewise use the fixed firmware.

## Current reported stock baseline

The supplied ShallotsOS report identifies one MIPS 74Kc CPU core online and
no CPU frequency control. Stock Snes9x 2005 reportedly uses Codescape GCC 6.3.0
with plain MIPS32/hard-float code and no DSP instructions. These are
agent-reported measurements, not measurements made by this build environment.

| Stock Yoshi's Island scene | Drawn frame | Skipped frame | Drawn frames per 5 s |
|---|---|---|---|
| Light | 12–13 ms | — | 300 / 300 |
| Heavy | 17–22 ms | 7–9 ms | 165–200 / 300 |

Stock video/audio callbacks reportedly take about 0.8/0.05 ms. The frontend
agent is separately investigating bilinear presentation, audio resampling
and 40–60 ms hitches. These are not necessarily inside the core's measurements.
Compare the same scene, skip ratio and measurement boundaries. A stock
retro_run measurement includes callbacks; compare with total measured
core-plus-callback cost, allowing for profiler overhead.

## Timed behavior in 0.4

Timed draws until a run starts **at least one full frame late**, then requests
a drawing skip. It obeys the configured limit with a hard maximum of **three**
consecutive skips. Cap 0 disables skipping. This matches the latest reported
stock policy, superseding 0.3's half-frame/two-skip policy.

The schedule follows the advertised NTSC/PAL frame rate. It resets after a
gap over 100 ms, reset, state/content load, mode/cap changes, disabled frontend
video, backwards/failed clocks or an excessive fast-forward lead. The frontend
high-resolution timer is used when available, otherwise a monotonic clock.
Timed neither sleeps nor throttles and cannot compensate for a frontend that
calls the core too slowly. Auto remains available as a separate policy.

## Controlled compiler comparisons

The default remains `-O2`, MIPS32r2 O32 hard-float, with no LTO. Hidden internal
visibility allows local binding while keeping the libretro API visible.
The GCC tune default remains `24kc` until device comparisons justify a change.

Keep the core variant, scene and frontend settings fixed. Use separate output
directories and test **one change at a time**:

```sh
python tools/build.py --target sf3000 --apu fast --cc /path/to/sf3000-gcc --sysroot /path/to/sysroot --output-dir build/baseline
python tools/build.py --target sf3000 --apu fast --cc /path/to/sf3000-gcc --sysroot /path/to/sysroot --cflags="-mtune=74kc" --output-dir build/tune74
python tools/build.py --target sf3000 --apu fast --cc /path/to/sf3000-gcc --sysroot /path/to/sysroot --cflags="-O3" --output-dir build/o3
```

Extra compiler/linker flags are appended to defaults and parsed as argument
lists, never executed by a shell. Use `--cflags="..."` / `--ldflags="..."`
for values beginning with a dash; options can be repeated. The compiler must
support the flags (GCC tuning flags are not interchangeable with Zig's).

Each core has a `.build.json` manifest containing its flags and revision.
Keep this with comparison logs. An archive can supply `SOURCE_REVISION`, or
firmware CI can pass `--revision=<source-commit>`. Unknown provenance is
reported as `unknown`, never guessed from the upstream base.

The passthrough can support PGO experiments by passing the appropriate
`-fprofile-generate` or `-fprofile-use` flags at both compilation and link.
An end-to-end PGO collection/rebuild workflow has not been validated here;
it is not enabled in the supplied binaries.
