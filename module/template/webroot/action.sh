#!/system/bin/sh
CONFIG_FILE="/data/adb/modules/zygisk_device_spoof/config/config.json"
mkdir -p "/data/adb/modules/zygisk_device_spoof/config" 2>/dev/null
if [ ! -f "$CONFIG_FILE" ]; then
    echo '{"apps":[]}' > "$CONFIG_FILE"
    chmod 644 "$CONFIG_FILE"
fi
if [ "$REQUEST_METHOD" = "POST" ]; then
    BODY=$(cat)
    if echo "$BODY" | grep -q "{"; then
        echo "$BODY" > "$CONFIG_FILE"
        chmod 644 "$CONFIG_FILE"
        echo '{"status":"ok"}'
    else
        echo '{"status":"error"}'
    fi
else
    cat "$CONFIG_FILE"
fi
