#!/system/bin/sh
MODDIR=/bin
MODID=zygisk_device_spoof
CONFIG_DIR="/data/adb/modules//config"
CONFIG_FILE="/config.json"
mkdir -p "" 2>/dev/null
if [ ! -f "" ]; then
    echo "{"apps":[]}" > ""
    chmod 644 ""
fi
if [ "$REQUEST_METHOD" = "POST" ]; then
    BODY=$(cat)
    if echo "$BODY" | grep -q "{"; then
        echo "$BODY" > ""
        chmod 644 ""
        echo "{"status":"ok"}"
    else
        echo "{"status":"error"}"
    fi
else
    cat ""
fi
