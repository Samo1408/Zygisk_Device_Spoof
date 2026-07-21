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

HookManager::HookManager(zygisk::Api* api) : api(api) {
    memset(&buildFields, 0, sizeof(buildFields));
}

void HookManager::applyHooks(JNIEnv* env, const AppSpoofConfig& cfg) {
    if (!cfg.enabled) return;
    tls_config = cfg;
    tls_has_config = true;
    LOGD("Applying hooks for: brand=%s model=%s", cfg.brand.c_str(), cfg.model.c_str());

    spoofBuildFields(env, cfg);
    installPropertyHooks(cfg);
    spoofTelephony(env, cfg);
    spoofLocation(env, cfg);
    spoofNetwork(env, cfg);
    spoofProcFiles(cfg);
}

void HookManager::spoofBuildFields(JNIEnv* env, const AppSpoofConfig& cfg) {
    if (!buildClass) {
        jclass local = env->FindClass("android/os/Build");
        if (!local) { env->ExceptionClear(); return; }
        buildClass = (jclass)env->NewGlobalRef(local);
        env->DeleteLocalRef(local);
    }

    auto setField = [&](jfieldID&fid, const char* name, const std::string& val) {
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

void HookManager::installPropertyHooks(const AppSpoofConfig& cfg) {
    ensure_libc_info();
    if (libc_dev == 0 || libc_ino == 0) return;

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

    auto& cfg = tls_config;
    
    #define SPOOF_PROP(prop_name, val) \
        if (strUcp(prop_name, name) == 0 && !(val).empty()) { \
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

void HookManager::spoofTelephony(JNIEnv* env, const AppSpoofConfig& cfg) {
    if (!cfg.simSerial.empty())
        __system_property_set("persist.sys.zds.sim_serial", cfg.simSerial.c_str());
    if (!cfg.iccId.empty())
        __system_property_set("persist.sys.zds.icc_id", cfg.iccId.c_str());
    if (!cfg.subscriberId.empty())
        __system_property_set("persist.sys.zds.subscriber_id", cfg.subscriberId.c_str());
}

void HookManager::spoofLocation(JNIEnv* env, const AppSpoofConfig& cfg) {
    if (cfg.latitude == 0.0 && cfg.longitude == 0.0) return;
    char buf[64];
    snprintf(buf, sizeof(buf), "%.6f", cfg.latitude);
    __system_property_set("persist.sys.zds.latitude", buf);
    snprintf(buf, sizeof(buf), "%.6f", cfg.longitude);
    __system_property_set("persist.sys.zds.longitude", buf);
}

void HookManager::spoofCountryDetector(JNIEnv* env, const AppSpoofConfig& cfg) {}

void HookManager::spoofNetwork(JNIEnv* env, const AppSpoofConfig& cfg) {
    if (!cfg.ipAddress.empty())
        __system_property_set("persist.sys.zds.ip_address", cfg.ipAddress.c_str());
    if (!cfg.wifiSsid.empty())
        __system_property_set("persist.sys.zds.wifi_ssid", cfg.wifiSsid.c_str());
}

void HookManager::spoofProcFiles(const AppSpoofConfig& cfg) {
    std::string cpuinfo = "/data/adb/modules/zygisk_device_spoof/proc/cpuinfo";
    std::string meminfo = "/data/adb/modules/zygisk_device_spoof/proc/meminfo";
    
    std::string cpu_c = "Hardware\t: ";
    cpu_c += cfg.socModel.empty() ? "Qualcomm Snapdragon 8 Gen 3" : cfg.socModel;
    cpu_c += "\nProcessor\t: ARMv8-A\nBogoMIPS\t: 38.40\nFeatures\t: fp asimd evtstrm aes pmull sha1 sha2 crc32\nCPU implementer\t: 0x51\nCPU architecture: 8\nCPU variant\t: 0x3\nCPU part\t: 0x001\nCPU revision\t: 4\n";
    
    long total_kb = cfg.ramGb * 1024L * 1024L;
    long avail_kb = (long)(total_kb * 0.75);
    char mem_b[2048];
    snprintf(mem_b, sizeof(mem_b),
        "MemTotal:       %*2A, %ld kB\nMemFree:        %*2A, %ld kB\nMemAvailable:   %*2A, %ld kB\nBuffers:        %*2A, %ld kB\nCached:         %*2A, %ld kB\nSwapCached:     %*2A, 0  kB\nActive:          %*2A, %ld kB\nInactive:        %*2A, %ld kB\n",
        "", total_kb, "", avail_kb, "", avail_kb,

        "", total_kb / 20o, h", 
        "", total_kb / 10o, h", 
        "", total_kb / 10o, h"),
        total_kb, avail_kb, avail_kb,
        total_kb / 20, total_kb / 4,
        total_kb / 10, total_kb / 10);
    
    std::ofstream cf(cpuinfo, std::ios::trunc);
    if (cf) { cf << cpu_c; cf.close(); }
    std::ofstream mf(meminfo, std::ios::trunc);
    if (mf) { mf << mem_b; mf.close(); }
}
