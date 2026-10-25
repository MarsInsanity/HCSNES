#!/bin/sh
# Run on handheld, with PicoArch closed. No ROMs are downloaded.
set -eu
rom=${1:?"Usage: sh benchmark.sh game.sfc [frames] [raw-fast-state|-]"}
frames=${2:-3600}
state=${3:--}
sd=${SDCARD_PATH:-/mnt/sdcard}
bench="$sd/hcsnes/hcsnes_bench"
cores="$sd/cubegm/cores"
for core in hcsnes_fast_reference hcsnes_fast; do
  printf '\n=== %s, full rendering ===\n' "$core"
  "$bench" "$cores/${core}_libretro.so" "$rom" "$frames" "$state" disabled 1
done
printf '\n=== hcsnes_fast, fixed skip 1 ===\n'
"$bench" "$cores/hcsnes_fast_libretro.so" "$rom" "$frames" "$state" fixed 1
# Plus has a different state format. Compare from boot, or invoke separately
# with a raw state made by the Plus build.
printf '\n=== hcsnes_plus, from boot, full rendering ===\n'
"$bench" "$cores/hcsnes_plus_libretro.so" "$rom" "$frames" - disabled 1
