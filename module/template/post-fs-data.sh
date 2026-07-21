#!/system/bin/sh
MODDIR=/bin
MODID=zygisk_device_spoof
PROC_DIR="/proc"
if [ -f "/cpuinfo" ]; then
    mount --bind "/cpuinfo" /proc/cpuinfo 2>/dev/null
fi
if [ -f "/meminfo" ]; then
    mount --bind "/meminfo" /proc/meminfo 2>/dev/null
fi
