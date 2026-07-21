#!/system/bin/sh
MODDIR=${0%/*}
if [ -f "$MODDIR/proc/cpuinfo" ]; then
    mount --bind "$MODDIR/proc/cpuinfo" /proc/cpuinfo 2>/dev/null
fi
if [ -f "$MODDIR/proc/meminfo" ]; then
    mount --bind "$MODDIR/proc/meminfo" /proc/meminfo 2>/dev/null
fi
