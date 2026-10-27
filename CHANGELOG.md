# HCSNES changelog

## 0.4.0

- Timed now requests a drawing skip at **one full frame of lateness**, with
  a hard maximum of **three consecutive skips**, following the latest stock
  probe report. It still obeys the user's lower configured cap; Auto/cap 1
  remain the defaults. Emulation and audio continue every frame.
- Profiling emits up to ten **HCSNES spike** records after each 300-frame
  summary (and partial-window flush). Each records the exact frame number,
  drawn/skipped status, exclusive subsystem/callback durations and that
  frame's GSU instruction/PLOT/RPIX counts. Largest wall-time frames come first;
  ties retain earlier frames. Storage is fixed-size with no per-frame logging.
- Optional calling-thread CPU time and an approximate off-CPU duration help
  distinguish CPU work from scheduling/blocking. The clock is sampled only
  while profiling; unavailability is explicit and does not disable profiling.
- Document the ShallotsOS background-polling fix: pre-fix HCSNES timings are
  not the current baseline. New 60-second samples must use the fixed firmware.
- Audit skipped-frame work: background line decoding and normal screen
  redraw/clip rebuilding are bypassed; required sprite overflow flags remain.
  No speculative renderer changes, GSU time slicing or worker threads.
- Deterministic tests cover full-frame thresholds, the three-skip ceiling,
  worst-frame ranking/ties, per-frame counters, short/empty groups, reset,
  missing CPU timing, and unchanged Fast/Plus execution and save layout.

For normal play try **hcsnes_fast / Timed / cap 3 / Profiling Off** on the
fixed ShallotsOS build. For the remaining spikes, record the same slow scene
for 60 seconds with Profiling On and send both core and frontend logs.
See [PROFILING.md](PROFILING.md). The 0.4 device improvement remains unverified.

## 0.3.0

- New **Timed** frameskip option alongside Auto. It skips drawing at the start
  of a frame when more than half a frame late, using the advertised NTSC/PAL
  frame rate. No audio-feedback dependency; emulation and audio still advance.
- Timed obeys the configured cap with a hard maximum of two consecutive skips.
  Pauses over 100 ms, reset, state/content load, mode/cap changes, disabled video
  and fast-forward lead clear stale timing. Clock failure/backwards time draws.
  Auto and cap 1 remain the defaults.
- Profiles add **core_drawn** and **core_skipped** average/maximum timings,
  each divided by its own frame count. Empty groups report 0/0. The existing
  aggregate core timing is retained.
- Document the corrected single-core hardware report and the agent's stock
  firmware baseline. No worker threads or emulation timing hacks are added.
- Deterministic policy and real-core tests cover deadline response, two-skip
  bound, missing audio feedback, NTSC/PAL rates, pause/reset/load, disabled
  video, clock failure and unchanged emulation/audio progression.
- Host timing state is outside the serialized format. Existing same-platform,
  same-audio-variant state layout is unchanged.

For normal play, try **hcsnes_fast / Timed / cap 2 / Profiling Off**. For the
next agent comparison, collect Auto and Timed logs as described in
[PROFILING.md](PROFILING.md). On-device Yoshi's Island gains remain unverified.

## 0.2.0

- Optional **Profiling: Off / On** logs one timing/counter line every 300
  emulated frames through the frontend's libretro logger. Off is the default.
- Exclusive timings separate core work from video, audio and input callbacks.
  GSU instructions and PLOT/RPIX operations are counted only while profiling.
- Core information and startup logs identify the HCSNES version and source
  revision. Partial profiling windows flush on Off, reset, state load or unload.
- Build controls: repeatable `--cflags` / `--ldflags`, separate
  `--output-dir`, and `--revision` for firmware/archive builds. The existing
  `--target sf3000 --apu fast|plus --cc ... --sysroot ...` interface still works.
- Internal symbols compile with hidden visibility; libretro entry points keep
  default visibility. No new instruction set or CPU timing hack is enabled.
- Reject undersized/null serialization buffers before writing.
- Emulated state structures and serialized layout are unchanged. Fast and Plus
  still have different state formats. Keep matching architecture and variant.
- Expanded tests cover profiling Off/On equivalence, counting, timing boundaries,
  logging cadence, missing logging support and previous-version state replay.

What to test on ShallotsOS: enable logging and Profiling, play the same slow
Yoshi's Island scene on Fast, Plus and Fast-reference, then send the core and
frontend logs. See [PROFILING.md](PROFILING.md). Device speed is not yet measured;
this release does not establish a frame-rate improvement.
