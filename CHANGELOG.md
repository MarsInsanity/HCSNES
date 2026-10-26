# HCSNES changelog

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
