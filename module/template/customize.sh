#!/system/bin/sh
MODDIR=/bin
MODID=zygisk_device_spoof
CONFIG_DIR="/data/adb/modules//config"
PROC_DIR="/data/adb/modules//proc"
mkdir -p "" 2>/dev/null
mkdir -p "" 2>/dev/null
if [ ! -f "/config.json" ]; then
    echo "{"apps":[]}" > "/config.json"
    chmod 644 "/config.json"
fi
chmod -R 755 ""
echo "Zygisk Device Spoof v2.0 installed!"
