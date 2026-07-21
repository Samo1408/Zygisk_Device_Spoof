#include "ConfigManager.h"
#include <fstream>
#include <sys/stat.h>
#include <android/log.h>

#ifdef DEBUG
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "ZygiskDeviceSpoof", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "ZygiskDeviceSpoof", __VA_ARGS__)
#else
#define LOGD(...)
#define LOGE(...)
#endif

const std::string ConfigManager::CONFIG_PATH = "/data/adb/modules/zygisk_device_spoof/config/config.json";

// --- Country Presets Data ---
static const std::vector<CountryPreset> s_country_presets = {
    {"us", "United States",    311, 480, "America/Chicago",      32.0, 48.0,  -125.0, -66.0,  "en"},
    {"gb", "United Kingdom",   234, 30,  "Europe/London",        50.0, 59.0,  -8.0,   2.0,   "en"},
    {"de", "Germany",          262, 2,   "Europe/Berlin",        47.0, 55.0,  5.0,   15.0,  "de"},
    {"ca", "Canada",           302, 720, "America/Toronto",      43.0, 60.0,  -140.0, -52.0, "en"},
    {"ch", "Switzerland",      228, 1,   "Europe/Zurich",        45.0, 48.0,  5.0,   11.0,  "de"},
    {"kr", "South Korea",      450, 5,   "Asia/Seoul",           33.0, 39.0,  125.0, 130.0, "ko"},
    {"jp", "Japan",            440, 10,  "Asia/Tokyo",           30.0, 46.0,  129.0, 146.0, "ja"},
    {"fr", "France",           208, 1,   "Europe/Paris",         42.0, 51.0,  -5.0,  8.0,   "fr"},
    {"au", "Australia",        505, 1,   "Australia/Sydney",     -38.0, -10.0, 113.0, 154.0, "en"},
    {"br", "Brazil",           724, 5,   "America/Sao_Paulo",    -34.0, 5.0,   -74.0, -34.0, "pt"},
    {"in", "India",            405, 45,  "Asia/Kolkata",          8.0,  37.0,  68.0,  98.0,  "hi"},
    {"ru", "Russia",           250, 1,   "Europe/Moscow",        41.0, 70.0,  20.0,  180.0, "ru"},
    {"it", "Italy",            222, 1,   "Europe/Rome",          36.0, 47.0,  6.0,   19.0,  "it"},
    {"es", "Spain",            214, 1,   "Europe/Madrid",        36.0, 44.0,  -10.0, 4.0,   "es"},
    {"nl", "Netherlands",      204, 4,   "Europe/Amsterdam",     50.0, 54.0,  3.0,   8.0,   "nl"},
    {"se", "Sweden",           240, 1,   "Europe/Stockholm",     55.0, 69.0,  11.0,  24.0,  "sv"},
    {"no", "Norway",           242, 1,   "Europe/Oslo",          57.0, 72.0,  4.0,   32.0,  "no"},
    {"dk", "Denmark",          238, 1,   "Europe/Copenhagen",    54.0, 58.0,  8.0,   16.0,  "da"},
    {"fi", "Finland",          244, 5,   "Europe/Helsinki",      59.0, 70.0,  20.0,  32.0,  "fi"},
    {"pl", "Poland",           260, 1,   "Europe/Warsaw",        49.0, 55.0,  14.0,  24.0,  "pl"},
    {"tr", "Turkey",           286, 1,   "Europe/Istanbul",      36.0, 42.0,  26.0,  45.0,  "tr"},
    {"sa", "Saudi Arabia",     420, 1,   "Asia/Riyadh",          16.0, 33.0,  34.0,  56.0,  "ar"},
    {"ae", "UAE",              424, 2,   "Asia/Dubai",           22.0, 26.0,  51.0,  57.0,  "ar"},
    {"sg", "Singapore",        525, 1,   "Asia/Singapore",       1.0,  2.0,   103.0, 104.0, "en"},
    {"mx", "Mexico",           334, 20,  "America/Mexico_City",  15.0, 33.0,  -118.0, -86.0, "es"},
    {"id", "Indonesia",        510, 8,   "Asia/Jakarta",         -11.0, 6.0,   95.0,  141.0, "id"},
    {"my", "Malaysia",         502, 12,  "Asia/Kuala_Lumpur",    1.0,  7.0,   99.0,  120.0, "ms"},
    {"th", "Thailand",         520, 0,   "Asia/Bangkok",         5.0,  21.0,  97.0,  106.0, "th"},
    {"vn", "Vietnam",          452, 1,   "Asia/Ho_Chi_Minh",     8.0,  24.0,  102.0, 110.0, "vi"},
    {"ph", "Philippines",      515, 1,   "Asia/Manila",          5.0,  19.0,  117.0, 127.0, "en"},
};

// --- Helper: strip quotes ---
static std::string strip_quotes(std::string_view sv) {
    if (sv.size() >= 2 && sv.front() == '"' && sv.back() == '"') {
        return std::string(sv.substr(1, sv.size() - 2));
    }
    return std::string(sv);
}

// --- Safe string extraction ---
static std::string safe_str(simdjson::dom::element& el, const char* key) {
    std::string_view val;
    if (el[key].get_string().get(val) == simdjson::SUCCESS) return std::string(val);
    return "";
}

static int safe_int(simdjson::dom::element& el, const char* key) {
    int64_t val;
    if (el[key].get_int64().get(val) == simdjson::SUCCESS) return (int)val;
    return -1;
}

static double safe_double(simdjson::dom::element& el, const char* key) {
    double val;
    if (el[key].get_double().get(val) == simdjson::SUCCESS) return val;
    return 0.0;
}

// --- Overloads for simdjson::dom::object ---
static std::string safe_str(simdjson::dom::object& obj, const char* key) {
    std::string_view val;
    if (obj[key].get_string().get(val) == simdjson::SUCCESS) return std::string(val);
    return "";
}

static int safe_int(simdjson::dom::object& obj, const char* key) {
    int64_t val;
    if (obj[key].get_int64().get(val) == simdjson::SUCCESS) return (int)val;
    return -1;
}

static double safe_double(simdjson::dom::object& obj, const char* key) {
    double val;
    if (obj[key].get_double().get(val) == simdjson::SUCCESS) return val;
    return 0.0;
}

// --- Country Presets ---
const std::vector<CountryPreset>& ConfigManager::getCountryPresets() {
    return s_country_presets;
}

std::optional<CountryPreset> ConfigManager::getCountryPreset(const std::string& iso) {
    for (const auto& cp : s_country_presets) {
        if (cp.iso == iso) return cp;
    }
    return std::nullopt;
}

// --- ConfigManager ---
ConfigManager::ConfigManager() : last_modified_time() {}

std::chrono::system_clock::time_point ConfigManager::getFileModTime() const {
    struct stat result;
    if (stat(CONFIG_PATH.c_str(), &result) == 0) {
        return std::chrono::system_clock::from_time_t(result.st_mtime);
    }
    return std::chrono::system_clock::time_point();
}

void ConfigManager::loadOrReloadConfig() {
    std::scoped_lock lock(config_mutex);
    auto current_mod_time = getFileModTime();
    if (current_mod_time == last_modified_time && !app_configs.empty()) return;

    simdjson::dom::parser parser;
    simdjson::dom::element doc;
    if (parser.load(CONFIG_PATH).get(doc)) {
        LOGE("Failed to load config.json");
        app_configs.clear();
        return;
    }
    parseConfig(doc);
    last_modified_time = current_mod_time;
}

bool ConfigManager::isTargetApp(const std::string& app_name) const {
    std::scoped_lock lock(config_mutex);
    return app_configs.count(app_name) > 0;
}

std::optional<AppSpoofConfig> ConfigManager::getConfigForApp(const std::string& app_name) const {
    std::scoped_lock lock(config_mutex);
    auto it = app_configs.find(app_name);
    if (it != app_configs.end()) return it->second;
    return std::nullopt;
}

void ConfigManager::parseConfig(simdjson::dom::element& doc) {
    app_configs.clear();
    simdjson::dom::array apps;
    if (doc["apps"].get_array().get(apps) != simdjson::SUCCESS) return;

    for (simdjson::dom::element app_element : apps) {
        std::string_view package_name;
        if (app_element["package"].get_string().get(package_name) != simdjson::SUCCESS) continue;

        AppSpoofConfig cfg;
        parseAppConfig(app_element, cfg);

        // Apply country preset if specified
        std::string country_iso = safe_str(app_element, "country");
        if (!country_iso.empty()) {
            auto preset = getCountryPreset(country_iso);
            if (preset) {
                if (cfg.mcc < 0) cfg.mcc = preset->mcc;
                if (cfg.mnc < 0) cfg.mnc = preset->mnc;
                if (cfg.operatorNumeric.empty()) {
                    char buf[16];
                    snprintf(buf, sizeof(buf), "%03d%03d", preset->mcc, preset->mnc);
                    cfg.operatorNumeric = buf;
                }
                cfg.countryIso = preset->iso;
                cfg.defaultTimezone = preset->timezone;
                // Random location within country bounds
                if (cfg.latitude == 0.0 && cfg.longitude == 0.0) {
                    cfg.latitude = preset->lat_min + (rand() / (double)RAND_MAX) * (preset->lat_max - preset->lat_min);
                    cfg.longitude = preset->lon_min + (rand() / (double)RAND_MAX) * (preset->lon_max - preset->lon_min);
                }
            }
        }

        app_configs.emplace(package_name, std::move(cfg));
    }
    LOGD("Loaded config for %zu apps.", app_configs.size());
}

void ConfigManager::parseAppConfig(simdjson::dom::element& app, AppSpoofConfig& cfg) {
    // Top-level fields
    cfg.enabled = true;
    bool en = false;
    if (app["enabled"].get_bool().get(en) == simdjson::SUCCESS) cfg.enabled = en;

    // Profile object
    simdjson::dom::object props;
    if (app["properties"].get_object().get(props) == simdjson::SUCCESS) {
        cfg.brand = safe_str(props, "brand");
        cfg.model = safe_str(props, "model");
        cfg.manufacturer = safe_str(props, "manufacturer");
        cfg.device = safe_str(props, "device");
        cfg.product = safe_str(props, "productName");
        cfg.board = safe_str(props, "board");
        cfg.hardware = safe_str(props, "hardware");
        cfg.buildFingerprint = safe_str(props, "buildFingerprint");
        cfg.buildId = safe_str(props, "buildId");
        cfg.buildDisplayId = safe_str(props, "buildDisplayId");
        cfg.buildIncremental = safe_str(props, "buildIncremental");
        cfg.securityPatch = safe_str(props, "securityPatch");
        cfg.buildDescription = safe_str(props, "buildDescription");
        cfg.buildFlavor = safe_str(props, "buildFlavor");
        cfg.buildProduct = safe_str(props, "buildProduct");
        cfg.bootloader = safe_str(props, "bootloader");
        cfg.baseband = safe_str(props, "baseband");
        cfg.buildHost = safe_str(props, "buildHost");
        cfg.buildUser = safe_str(props, "buildUser");
        cfg.buildTags = safe_str(props, "buildTags");
        
        cfg.socModel = safe_str(props, "socModel");
        cfg.socManufacturer = safe_str(props, "socManufacturer");
    }

    // Telephony section
    simdjson::dom::object tele;
    if (app["telephony"].get_object().get(tele) == simdjson::SUCCESS) {
        cfg.iccOperatorIsoCountry = safe_str(tele, "iccOperatorIsoCountry");
        cfg.iccOperatorNumeric = safe_str(tele, "iccOperatorNumeric");
        cfg.operatorIsoCountry = safe_str(tele, "operatorIsoCountry");
        cfg.operatorNumeric = safe_str(tele, "operatorNumeric");
        cfg.simSerial = safe_str(tele, "simSerial");
        cfg.iccId = safe_str(tele, "iccId");
        cfg.subscriberId = safe_str(tele, "subscriberId");
        cfg.carrierId = safe_int(tele, "carrierId");
        cfg.carrierName = safe_str(tele, "carrierName");
        cfg.mcc = safe_int(tele, "mcc");
        cfg.mnc = safe_int(tele, "mnc");
    }

    // Identifiers section
    simdjson::dom::object ids;
    if (app["identifiers"].get_object().get(ids) == simdjson::SUCCESS) {
        cfg.androidId = safe_str(ids, "androidId");
        cfg.gsfId = safe_str(ids, "gsfId");
        cfg.appSetId = safe_str(ids, "appSetId");
        cfg.mediaDrmId = safe_str(ids, "mediaDrmId");
    }

    // Network section
    simdjson::dom::object net;
    if (app["network"].get_object().get(net) == simdjson::SUCCESS) {
        cfg.ipAddress = safe_str(net, "ipAddress");
        cfg.wifiSsid = safe_str(net, "wifiSsid");
    }

    // Location section
    simdjson::dom::object loc;
    if (app["location"].get_object().get(loc) == simdjson::SUCCESS) {
        cfg.latitude = safe_double(loc, "latitude");
        cfg.longitude = safe_double(loc, "longitude");
    }

    // Display section
    simdjson::dom::object disp;
    if (app["display"].get_object().get(disp) == simdjson::SUCCESS) {
        cfg.userAgent = safe_str(disp, "userAgent");
        cfg.ramGb = safe_int(disp, "ramGb");
        if (cfg.ramGb <= 0) cfg.ramGb = 12;
    }

    // Country override from top-level
    std::string country = safe_str(app, "defaultTimezone");
    if (!country.empty()) cfg.defaultTimezone = country;
}
