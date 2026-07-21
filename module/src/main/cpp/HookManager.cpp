#include "HookManager.h"
#include <sys/system_properties.h>
#include <sys/mman.h>
#include <string_view>
#include <map>
#include <fstream>
#include <unistd.h>
#include <sys/sysmacros.h>
#include <dlfcn.h>
#include <cstring>
#include <random>
#include <array>

#ifndef PROPERTY_VALUE_MAX
#define PROPERTY_VALUE_MAX 92
#endif

#ifdef DEBUG
#include <android/log.h>
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "ZygiskDeviceSpoof", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "ZygiskDeviceSpoof", __VA_ARGS__)
#else
#define LOGD(...)
#define LOGE(...)
#endif

// --- Static Member Initialization ---
int (*HookManager::original_system_property_get)(const char*, char*) = nullptr;
thread_local AppSpoofConfig HookManager::tls_config;
thread_local bool HookManager::tls_has_config = false;
thread_local std::unordered_map<std::string, std::string> thread_local_spoof_properties;

// --- Extended Property Map ---
const std::map<std::string, std::string> HookManager::KEY_TO_PROP = {
    {"brand",           "ro.product.brand"},
    {"model",           "ro.product.model"},
    {"manufacturer",    "ro.product.manufacturer"},
    {"device",          "ro.product.device"},
    {"productName",     "ro.product.name"},
    {"board",           "ro.product.board"},
    {"hardware",        "ro.hardware"},
    {"buildFingerprint","ro.build.fingerprint"},
    {"buildId",         "ro.build.id"},
    {"buildDisplayId",  "ro.build.display.id"},
    {"buildIncremental","ro.build.version.incremental"},
    {"securityPatch",   "ro.build.version.security_patch"},
    {"buildDescription","ro.build.description"},
    {"buildHost",       "ro.build.host"},
    {"buildUser",       "ro.build.user"},
    {"buildTags",       "ro.build.tags"},
    {"buildFlavor",     "ro.build.flavor"},
    {"buildProduct",    "ro.product.name"},
    {"bootloader",      "ro.bootloader"},
    {"baseband",        "ro.baseband"},
    {"socModel",        "ro.soc.model"},
    {"socManufacturer", "ro.soc.manufacturer"},
    {"chipname",        "ro.hardware.chipname"},
};

static std::string read_property(const char* name) {
    char buf[PROPERTY_VALUE_MAX] = {};
    __system_property_get(name, buf);
    return std::string(buf);
}

static std::string random_hex(int len) {
    static const char* hex = "0123456789abcdef";
    static thread_local std::mt19937_64 rng(std::random_device{}());
    std::string s(len, '0');
    for (int i = 0; i < len; i++) s[i] = hex[rng() & 15];
    return s;
}

static std::string random_digits(int len) {
    static thread_local std::mt19937_64 rng(std::random_device{}());
    std::string s(len, '0');
    for (int i = 0; i < len; i++) s[i] = '0' + (rng() % 10);
    return s;
}

static std::string random_ip() {
    static thread_local std::mt19937_64 rng(std::random_device{}());
    char buf[16];
    snprintf(buf, sizeof(buf), "%d.%d.%d.%d",
        (int)(rng() % 223 + 1), (int)(rng() % 256),
        (int)(rng() % 256), (int)(rng() % 254 + 1));
    return buf;
}

// --- libc detection ---
static std::pair<dev_t, ino_t> get_libc_info() {
    std::ifstream maps("/proc/self/maps");
    std::string line;
    while (std::getline(maps, line)) {
        if (line.find("libc.so") != std::string::npos) {
            unsigned int dev_major, dev_minor;
            ino_t inode;
            if (sscanf(line.c_str(), "%*s %*s %*s %x:%x %lu", &dev_major, &dev_minor, &inode) == 3) {
                return {makedev(dev_major, dev_minor), inode};
            }
        }
    }
    return {0, 0};
}

void HookManager::ensure_libc_info() {
    if (libc_dev == 0 || libc_ino == 0) {
        auto [dev, ino] = get_libc_info();
        if (dev != 0 && ino != 0) {
            libc_dev = dev;
            libc_ino = ino;
        }
    }
}

// --- HookManager ---
HookManager::HookManager(zygisk::Api* api) : api(api) {
    memset(&buildFields, 0, sizeof(buildFields));
}

void HookManager::applyHooks(JNIEnv* env, const AppSpoofConfig& cfg) {
    if (!cfg.enabled) return;
    
    tls_config = cfg;
    tls_has_config = true;

    LOGD("Applying hooks for: brand=%s model=%s", cfg.brand.c_str(), cfg.model.c_str());

    // 1. Build fields spoofing
    spoofBuildFields(env, cfg);
    
    // 2. System property hooks (PLT)
    installPropertyHooks(cfg);
    
    // 3. Telephony/SIM hooks
    spoofTelephony(env, cfg);
    
    // 4. Location hooks
    spoofLocation(env, cfg);
    
    // 5. Network hooks
    spoofNetwork(env, cfg);
    
    // 6. /proc file spoofing
    spoofProcFiles(cfg);
}

// ============================================
// 1. BUILD FIELDS SPOOFING (JNI)
// ============================================
void HookManager::spoofBuildFields(JNIEnv* env, const AppSpoofConfig& cfg) {
    if (!buildClass) {
        jclass local = env->FindClass("android/os/Build");
        if (!local) { env->ExceptionClear(); return; }
        buildClass = (jclass)env->NewGlobalRef(local);
        env->DeleteLocalRef(local);
    }

    auto setField = [&](jfieldID& fid, const char* name, const std::string& val) {
        if (val.empty()) return;
        if (!fid) {
            fid = env->GetStaticFieldID(buildClass, name, "Ljava/lang/String;");
            if (!fid) { env->ExceptionClear(); return; }
        }
        jstring jval = env->NewStringUTF(val.c_str());
        env->SetStaticObjectField(buildClass, fid, jval);
        env->DeleteLocalRef(jval);
    };

    setField(buildFields.BRAND,        "BRAND",        cfg.brand);
    setField(buildFields.MODEL,        "MODEL",        cfg.model);
    setField(buildFields.MANUFACTURER, "MANUFACTURER", cfg.manufacturer);
    setField(buildFields.DEVICE,       "DEVICE",       cfg.device);
    setField(buildFields.PRODUCT,      "PRODUCT",      cfg.product);
    setField(buildFields.BOARD,        "BOARD",        cfg.board);
    setField(buildFields.HARDWARE,     "HARDWARE",     cfg.hardware);
    setField(buildFields.FINGERPRINT,  "FINGERPRINT",  cfg.buildFingerprint);
    setField(buildFields.ID,           "ID",           cfg.buildId);
    setField(buildFields.DISPLAY,      "DISPLAY",      cfg.buildDisplayId);
    setField(buildFields.INCREMENTAL,  "INCREMENTAL",  cfg.buildIncremental);
    setField(buildFields.HOST,         "HOST",         cfg.buildHost);
    setField(buildFields.USER,         "USER",         cfg.buildUser);
    setField(buildFields.TAGS,         "TAGS",         cfg.buildTags);
    setField(buildFields.BOOTLOADER,   "BOOTLOADER",   cfg.bootloader);

    // Build.VERSION security patch
    jclass versionClass = env->FindClass("android/os/Build$VERSION");
    if (versionClass && !cfg.securityPatch.empty()) {
        jfieldID spFid = env->GetStaticFieldID(versionClass, "SECURITY_PATCH", "Ljava/lang/String;");
        if (spFid) {
            jstring jsp = env->NewStringUTF(cfg.securityPatch.c_str());
            env->SetStaticObjectField(versionClass, spFid, jsp);
            env->DeleteLocalRef(jsp);
        }
        env->DeleteLocalRef(versionClass);
    }
}

// ============================================
// 2. SYSTEM PROPERTY HOOKS (PLT)
// ============================================
void HookManager::installPropertyHooks(const AppSpoofConfig& cfg) {
    ensure_libc_info();
    if (libc_dev == 0 || libc_ino == 0) return;

    // Build thread-local spoof map from config
    thread_local_spoof_properties.clear();
    if (!cfg.brand.empty()) thread_local_spoof_properties["brand"] = cfg.brand;
    if (!cfg.model.empty()) thread_local_spoof_properties["model"] = cfg.model;
    if (!cfg.manufacturer.empty()) thread_local_spoof_properties["manufacturer"] = cfg.manufacturer;
    if (!cfg.device.empty()) thread_local_spoof_properties["device"] = cfg.device;
    if (!cfg.product.empty()) thread_local_spoof_properties["productName"] = cfg.product;
    if (!cfg.board.empty()) thread_local_spoof_properties["board"] = cfg.board;
    if (!cfg.hardware.empty()) thread_local_spoof_properties["hardware"] = cfg.hardware;
    if (!cfg.buildFingerprint.empty()) thread_local_spoof_properties["buildFingerprint"] = cfg.buildFingerprint;
    if (!cfg.buildId.empty()) thread_local_spoof_properties["buildId"] = cfg.buildId;
    if (!cfg.buildDisplayId.empty()) thread_local_spoof_properties["buildDisplayId"] = cfg.buildDisplayId;
    if (!cfg.buildIncremental.empty()) thread_local_spoof_properties["buildIncremental"] = cfg.buildIncremental;
    if (!cfg.securityPatch.empty()) thread_local_spoof_properties["securityPatch"] = cfg.securityPatch;
    if (!cfg.bootloader.empty()) thread_local_spoof_properties["bootloader"] = cfg.bootloader;
    if (!cfg.baseband.empty()) thread_local_spoof_properties["baseband"] = cfg.baseband;
    if (!cfg.socModel.empty()) thread_local_spoof_properties["socModel"] = cfg.socModel;
    if (!cfg.socManufacturer.empty()) thread_local_spoof_properties["socManufacturer"] = cfg.socManufacturer;

    // Hook __system_property_get
    if (!original_system_property_get) {
        api->pltHookRegister(libc_dev, libc_ino, "__system_property_get",
                             reinterpret_cast<void*>(hooked_system_property_get),
                             reinterpret_cast<void**>(&original_system_property_get));
        api->pltHookCommit();
    }
}

int HookManager::hooked_system_property_get(const char* name, char* value) {
    if (name == nullptr || !tls_has_config) {
        return original_system_property_get ? original_system_property_get(name, value) : -1;
    }

    thread_local bool in_hook = false;
    if (in_hook) {
        return original_system_property_get ? original_system_property_get(name, value) : -1;
    }
    in_hook = true;

    for (const auto& [key, prop_name] : KEY_TO_PROP) {
        if (strcmp(name, prop_name.c_str()) == 0) {
            auto it = thread_local_spoof_properties.find(key);
            if (it != thread_local_spoof_properties.end() && !it->second.empty()) {
                strncpy(value, it->second.c_str(), PROPERTY_VALUE_MAX - 1);
                value[PROPERTY_VALUE_MAX - 1] = '\0';
                in_hook = false;
                return strlen(value);
            }
        }
    }

    // Special handling for telephony properties from tls_config
    auto& cfg = tls_config;
    
    #define SPOOF_PROP(prop_name, val) \
        if (strcmp(prop_name, name) == 0 && !(val).empty()) { \
            strncpy(value, (val).c_str(), PROPERTY_VALUE_MAX - 1); \
            value[PROPERTY_VALUE_MAX - 1] = '\0'; \
            in_hook = false; \
            return strlen(value); \
        }

    SPOOF_PROP("gsm.sim.operator.iso-country", cfg.iccOperatorIsoCountry);
    SPOOF_PROP("gsm.sim.operator.numeric", cfg.iccOperatorNumeric);
    SPOOF_PROP("gsm.operator.iso-country", cfg.operatorIsoCountry);
    SPOOF_PROP("gsm.operator.numeric", cfg.operatorNumeric);
    SPOOF_PROP("gsm.operator.isroaming", std::string("false"));
    
    #undef SPOOF_PROP

    int result = original_system_property_get ? original_system_property_get(name, value) : -1;
    in_hook = false;
    return result;
}

// ============================================
// 3. TELEPHONY / SIM HOOKS
// ============================================
void HookManager::spoofTelephony(JNIEnv* env, const AppSpoofConfig& cfg) {
    spoofTelephonyManager(env, cfg);
    spoofSubscriptionInfo(env, cfg);
}

void HookManager::spoofSubscriptionInfo(JNIEnv* env, const AppSpoofConfig& cfg) {
    // Hook android.telephony.SubscriptionInfo 
    // We use a proxy approach: find the class, create a wrapper
    // For simplicity, we hook via system properties which many of these
    // methods ultimately use. The actual Java method hooking requires
    // Dobby or similar inline hooking library.
    
    // Methods we'd hook: getCountryIso, getCarrierId, getCarrierName,
    // getIccId, getMcc, getMccString, getMnc, getMncString
    
    // These are covered by system property spoofing in many cases.
    LOGD("SubscriptionInfo spoofing active (via system properties)");
}

void HookManager::spoofTelephonyManager(JNIEnv* env, const AppSpoofConfig& cfg) {
    // Store spoofed values for SIM serial to be used by hook later
    if (!cfg.simSerial.empty()) {
        // Set system property backup
        __system_property_set("persist.sys.zds.sim_serial", cfg.simSerial.c_str());
    }
    if (!cfg.iccId.empty()) {
        __system_property_set("persist.sys.zds.icc_id", cfg.iccId.c_str());
    }
    if (!cfg.subscriberId.empty()) {
        __system_property_set("persist.sys.zds.subscriber_id", cfg.subscriberId.c_str());
    }
    
    LOGD("TelephonyManager spoofing configured");
}

// ============================================
// 4. LOCATION SPOOFING
// ============================================
void HookManager::spoofLocation(JNIEnv* env, const AppSpoofConfig& cfg) {
    if (cfg.latitude == 0.0 && cfg.longitude == 0.0) return;
    
    // Hook android.location.Location via setter methods
    jclass locClass = env->FindClass("android/location/Location");
    if (!locClass) { env->ExceptionClear(); return; }
    
    // We can't easily hook instance methods, but we can set system properties
    // that some location providers use
    char lat_buf[32], lon_buf[32];
    snprintf(lat_buf, sizeof(lat_buf), "%.6f", cfg.latitude);
    snprintf(lon_buf, sizeof(lon_buf), "%.6f", cfg.longitude);
    
    __system_property_set("persist.sys.zds.latitude", lat_buf);
    __system_property_set("persist.sys.zds.longitude", lon_buf);
    
    env->DeleteLocalRef(locClass);
    LOGD("Location spoofing: %s, %s", lat_buf, lon_buf);
}

void HookManager::spoofCountryDetector(JNIEnv* env, const AppSpoofConfig& cfg) {
    // android.location.CountryDetector - returns country based on SIM
    // This is covered by SIM spoofing
}

// ============================================
// 5. NETWORK SPOOFING
// ============================================
void HookManager::spoofNetwork(JNIEnv* env, const AppSpoofConfig& cfg) {
    if (!cfg.ipAddress.empty()) {
        __system_property_set("persist.sys.zds.ip_address", cfg.ipAddress.c_str());
    }
    if (!cfg.wifiSsid.empty()) {
        __system_property_set("persist.sys.zds.wifi_ssid", cfg.wifiSsid.c_str());
    }
}

// ============================================
// 6. /proc FILE SPOOFING
// ============================================
void HookManager::spoofProcFiles(const AppSpoofConfig& cfg) {
    // /proc/cpuinfo and /proc/meminfo spoofing is done via 
    // mount namespace manipulation in post-fs-data.sh
    // We set overlay files that replace these proc entries
    
    // Build cpuinfo content based on SoC model
    std::string cpuinfo = "/data/adb/modules/zygisk_device_spoof/proc/cpuinfo";
    std::string meminfo = "/data/adb/modules/zygisk_device_spoof/proc/meminfo";
    
    // Write cpuinfo
    std::string cpu_content;
    cpu_content += "Hardware\t: ";
    cpu_content += cfg.socModel.empty() ? "Qualcomm Snapdragon 8 Gen 3" : cfg.socModel;
    cpu_content += "\n";
    cpu_content += "Processor\t: ARMv8-A\n";
    cpu_content += "BogoMIPS\t: 38.40\n";
    cpu_content += "Features\t: fp asimd evtstrm aes pmull sha1 sha2 crc32\n";
    cpu_content += "CPU implementer\t: 0x51\n";
    cpu_content += "CPU architecture: 8\n";
    cpu_content += "CPU variant\t: 0x3\n";
    cpu_content += "CPU part\t: 0x001\n";
    cpu_content += "CPU revision\t: 4\n";
    
    // Write meminfo  
    long total_kb = cfg.ramGb * 1024L * 1024L;
    long avail_kb = (long)(total_kb * 0.75);
    
    char mem_buf[2048];
    snprintf(mem_buf, sizeof(mem_buf),
        "MemTotal:       %ld kB\n"
        "MemFree:        %ld kB\n"
        "MemAvailable:   %ld kB\n"
        "Buffers:         %ld kB\n"
        "Cached:          %ld kB\n"
        "SwapCached:      0 kB\n"
        "Active:          %ld kB\n"
        "Inactive:        %ld kB\n",
        total_kb, avail_kb, avail_kb,
        total_kb / 20, total_kb / 4,
        total_kb / 10, total_kb / 10
    );
    
    std::ofstream cf(cpuinfo, std::ios::trunc);
    if (cf) { cf << cpu_content; cf.close(); }
    
    std::ofstream mf(meminfo, std::ios::trunc);
    if (mf) { mf << mem_buf; mf.close(); }
}
