#!/bin/sh
set -eu
rom=${1:?"Usage: sh launch-hcsnes.sh /path/game.sfc [fast|plus|reference]"}
variant=${2:-fast}
case "$variant" in
  fast) core=hcsnes_fast_libretro.so ;;
  plus) core=hcsnes_plus_libretro.so ;;
  reference) core=hcsnes_fast_reference_libretro.so ;;
  *) echo "Unknown variant: $variant" >&2; exit 2 ;;
esac
sd=${SDCARD_PATH:-/mnt/sdcard}
frontend="$sd/cubegm/picoarch"
core_path="$sd/cubegm/cores/$core"
[ -x "$frontend" ] || { echo "PicoArch not found: $frontend" >&2; exit 1; }
[ -f "$core_path" ] || { echo "Core not found: $core_path" >&2; exit 1; }
[ -f "$rom" ] || { echo "ROM not found: $rom" >&2; exit 1; }
exec "$frontend" "$core_path" "$rom"
