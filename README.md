# Zygisk Device Spoof v2.0

Fork of the original Zygisk Device Spoof module with comprehensive enhancements:

- 🌍 Country-based presets (25+ countries)
- 📞 SIM/Telephony spoofing (operator, ICC, SIM serial, IMEI)
- 🌐 Network spoofing (IP, WiFi SSID)
- 🆔 Identifier spoofing (Android ID, GSF ID, MediaDrm)
- 🖥️ Extended device props (bootloader, baseband, SoC, security patch)
- 📱 WebUI for KernelSU (full configuration interface)
- 🗺️ Location spoofing (GPS coordinates)
- 💾 /proc/cpuinfo & /proc/meminfo spoofing
- ⚡ Dynamic config reload (no reboot needed)

## Building

```bash
./gradlew :module:zipDebug    # Debug build
./gradlew :module:zipRelease  # Release build
```

Output: `module/release/zygisk_device_spoof_*.zip`

## WebUI

Open KernelSU Manager → Zygisk Device Spoof → WebUI

Or visit: `http://127.0.0.1:<port>` in your browser when the WebUI is active.

## Config Format

```json
{
  "apps": [
    {
      "package": "com.example.app",
      "enabled": true,
      "country": "us",
      "properties": {
        "brand": "Google",
        "model": "Pixel 9 Pro",
        "manufacturer": "Google",
        "deviceCode": "komodo",
        "buildFingerprint": "google/komodo/komodo:15/..."
      },
      "telephony": {
        "mcc": 311,
        "mnc": 480,
        "simSerial": "89014103211118510720",
        "iccId": "89014103211118510720",
        "carrierName": "Google Fi"
      },
      "identifiers": {
        "androidId": "a1b2c3d4e5f6a7b8"
      },
      "network": {
        "ipAddress": "192.168.1.100",
        "wifiSsid": "HomeWiFi_5G"
      },
      "location": {
        "latitude": 37.7749,
        "longitude": -122.4194
      },
      "display": {
        "ramGb": 12,
        "userAgent": "Mozilla/5.0..."
      },
      "defaultTimezone": "America/Chicago"
    }
  ]
}
```

## License

Original module by Qjj0204. This fork is maintained by Samo1408. MIT License.
