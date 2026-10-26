# Measuring slow scenes on ShallotsOS

## No device shell required

1. Shut down the handheld and put its SD card in your computer.
2. Back up your saves and the existing HCSNES core files. Copy the package's
   **sdcard** contents to the SD-card root, replacing the HCSNES files.
3. Create an empty **log.txt** at the SD-card root. On Windows, enable
   **File name extensions** and check that the name is not `log.txt.txt`.
4. Boot ShallotsOS, select **hcsnes_fast** for Yoshi's Island, and turn
   **Profiling** to **On** in the core options. Keep **Frameskip = Auto**,
   **Maximum Consecutive Skips = 1**, CPU overclock hacks Off.
5. Play the slow scene for 30–60 seconds after entering gameplay.
6. Exit the game normally, shut down, and copy **logs/cores.log** and
   **logs/picoarch.log** from the SD card to your computer. Name the copies
   after the core and scene so a later run cannot overwrite the comparison.
7. Repeat with **hcsnes_plus**, and optionally **hcsnes_fast_reference**.
   Compare the same scene and settings. Fast and Plus cannot share raw states.
8. Turn Profiling Off for normal play. Remove the logging marker if desired.

The logging marker and paths above come from the ShallotsOS integration
handoff. Other PicoArch builds may use different logging controls. HCSNES
writes through `RETRO_ENVIRONMENT_GET_LOG_INTERFACE`; it never opens or
overwrites a device log itself. If the frontend supplies no logger, profiling
stays inactive. A startup `HCSNES build` line identifies the running binary.

Useful scenes: a repeatable Yoshi's Island zoom/rotation scene, Star Fox's first
level, and a non-Super FX game as a baseline. Write down the exact scene,
region and core choice. Do not use an intro as a substitute for slow gameplay.

## Reading a profile line

Each `HCSNES profile` line describes a window of 300 emulated frames.
Partial windows are also emitted when profiling is disabled, a state is loaded,
the game is reset, or the game is unloaded. `session` and `frame` identify
the current measurement sequence; a reset/state load starts a new session.

All `us_avg/max` pairs are **average / maximum microseconds per emulated
frame in that window**. A maximum is computed after summing all calls in one
frame. The timings are exclusive: nested work is not counted twice.

| Field | Meaning |
|---|---|
| `core` | Core execution, excluding frontend input/video/audio callbacks |
| `cpu_other` | Main-loop time after measured GSU, PPU and APU work is removed |
| `gsu` | Super FX execution, including opcode counting overhead |
| `ppu` | RenderLine plus S9xUpdateScreen, including its RenderScreen calls |
| `apu` | Audio mixing/output preparation; Plus also includes batched SPC/DSP execution and port synchronization |
| `other` | Remaining core-side frame setup/bookkeeping |
| `video_cb`, `audio_cb`, `input_cb` | Time spent in the respective frontend callbacks |
| `gsu_inst`, `plot`, `rpix` | Exact instruction/operation totals for this window |
| `rendered`, `skipped` | Frames submitted with an image versus duplicate/no-image frames |
| `feedback`, `underruns` | Frames with active audio-buffer feedback, and active frames reporting underrun risk |
| `occupancy` | Last reported audio-ring occupancy percentage, not a window average |
| `skip_mode`, `cap` | 0=disabled, 1=Auto, 2=Threshold, 3=Fixed; consecutive skip limit |
| `elapsed_ms` | Wall time since the profiling session started; includes pauses/menus |

**Fast's lockstep SPC700 interpreter remains inside `cpu_other`.** Timing every
SPC instruction would overwhelm the measurement. Likewise `cpu_other` includes
DMA, SA-1 and other uninstrumented work; it is not a pure 65816 measurement.
PPU register handling outside rendering also remains there.

Clock reads and GSU counting add overhead. Use these logs to locate expensive
work, then compare normal-play FPS with profiling Off. Do not interpret the
profiled run as a clean performance benchmark. `elapsed_ms` is not an FPS
counter. Logging/formatting costs after a completed frame are not charged to
that frame. Other threads' presentation work may happen outside the callback.

For normal NTSC timing a frame has roughly 16.6 ms. A large `core` value points
toward emulation work; expensive callbacks point toward frontend work. Align
these records with ShallotsOS's `DBG lag:` records in `picoarch.log` before
drawing conclusions. A high `cpu_other` in Fast does not by itself prove that
the 65816 is the bottleneck.

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
