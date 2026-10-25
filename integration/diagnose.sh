#!/bin/sh
# Read-only device information.
printf '=== system ===\n'
uname -a
printf '\n=== cpu ===\n'
cat /proc/cpuinfo
printf '\n=== memory ===\n'
head -8 /proc/meminfo
printf '\n=== firmware files ===\n'
ls -l /mnt/sdcard/cubegm/picoarch /mnt/sdcard/cubegm/cores/hcsnes* 2>/dev/null
printf '\n=== runtime libraries ===\n'
ls -l /lib/libc.so* /lib/ld*.so* /usr/lib/libc.so* 2>/dev/null
printf '\n=== clocks, if exported ===\n'
for f in /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor; do
  if [ -r "$f" ]; then printf '%s: ' "$f"; cat "$f"; fi
done
