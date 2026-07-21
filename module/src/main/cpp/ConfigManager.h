#pragma once

#include "simdjson.h"
#include <mutex>
#include <optional>
#include <string>
#include <sys/stat.h>
#include <unordered_map>
#include <functional>
#include <chrono>
#include <vector>

// --- Country Preset Structure ---
struct CountryPreset {
    std::string iso;
    std::string name;
    int mcc;
    int mnc;
    std::string timezone;
    double lat_min, lat_max, lon_min, lon_max;
    std::string language;
};

// --- App Spoof Config ---
struct AppSpoofConfig {
    // Device Identity
    std::string brand, model, manufacturer, device, product, board, hardware;
    std::string buildFingerprint, buildId, buildDisplayId, buildIncremental;
    std::string securityPatch, buildDescription, buildFlavor, buildProduct;
    std::string buildHost, buildUser, buildTags, bootloader, baseband;
    
    // SoC
    std::string socModel, socManufacturer;
    
    // SIM / Telephony
    std::string iccOperatorIsoCountry, iccOperatorNumeric;
    std::string operatorIsoCountry, operatorNumeric;
    std::string simSerial, iccId, subscriberId;
    int carrierId = -1;
    std::string carrierName;
    int mcc = -1, mnc = -1;
    
    // Country / Location
    std::string countryIso, countryCode;
    double latitude = 0.0, longitude = 0.0;
    
    // Network
    std::string ipAddress, wifiSsid;
    
    // Identifiers
    std::string androidId, gsfId, appSetId, mediaDrmId;
    
    // Extra
    std::string userAgent;
    int ramGb = 12;
    std::string defaultTimezone;
    
    bool enabled = true;
};

class ConfigManager {
public:
    ConfigManager();

    void loadOrReloadConfig();
    bool isTargetApp(const std::string& app_name) const;
    std::optional<AppSpoofConfig> getConfigForApp(const std::string& app_name) const;
    
    // Country presets
    static const std::vector<CountryPreset>& getCountryPresets();
    static std::optional<CountryPreset> getCountryPreset(const std::string& iso);

private:
    void parseConfig(simdjson::dom::element& doc);
    void parseAppConfig(simdjson::dom::element& app, AppSpoofConfig& cfg);
    std::chrono::system_clock::time_point getFileModTime() const;

    static const std::string CONFIG_PATH;
    std::chrono::system_clock::time_point last_modified_time;
    std::unordered_map<std::string, AppSpoofConfig> app_configs;
    mutable std::mutex config_mutex;
};
