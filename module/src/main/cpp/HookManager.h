#pragma once

#include "zygisk.hpp"
#include "ConfigManager.h"
#include <jni.h>
#include <string>
#include <sys/types.h>
#include <unordered_map>
#include <map>
#include <mutex>

class HookManager {
public:
    explicit HookManager(zygisk::Api* api);


    void applyHooks(JNIEnv* env, const AppSpoofConfig& config);

private:
    // --- Device Identity ---
    void spoofBuildFields(JNIEnv* env, const AppSpoofConfig& cfg);
    void installPropertyHooks(const AppSpoofConfig& cfg);
    
    // --- SIM / Telephony ---
    void spoofTelephony(JNIEnv* env, const AppSpoofConfig& cfg);
    void spoofSubscriptionInfo(JNIEnv* env, const AppSpoofConfig& cfg);
    void spoofTelephonyManager(JNIEnv* env, const AppSpoofConfig& cfg);
    
    // --- Country / Location ---
    void spoofLocation(JNIEnv* env, const AppSpoofConfig& cfg);
    void spoofCountryDetector(JNIEnv* env, const AppSpoofConfig& cfg);
    
    // --- Network ---
    void spoofNetwork(JNIEnv* env, const AppSpoofConfig& cfg);
    
    // --- /proc files ---
    void spoofProcFiles(const AppSpoofConfig& cfg);
    
    // --- Native hooks ---
    void ensure_libc_info();
    
    // --- Hooked functions ---
    static int hooked_system_property_get(const char* name, char* value);

    zygisk::Api* api;
    dev_t libc_dev = 0;
    ino_t libc_ino = 0;

    // Cached field IDs
    struct BuildFields {
        jfieldID BRAND, MODEL, MANUFACTURER, DEVICE, PRODUCT, BOARD, HARDWARE;
        jfieldID FINGERPRINT, ID, DISPLAY, INCREMENTAL, HOST, USER, TAGS;
        jfieldID BOOTLOADER;
        jfieldID SERIAL;
    } buildFields;

    // Cached method IDs for telephony
    jmethodID getSimSerialNumber = nullptr;
    jmethodID getDeviceId = nullptr;
    jmethodID getSubscriberId = nullptr;
    jmethodID getNetworkOperator = nullptr;
    jmethodID getNetworkOperatorName = nullptr;
    jmethodID getSimOperator = nullptr;
    jmethodID getSimOperatorName = nullptr;
    jmethodID getSimCountryIso = nullptr;
    jmethodID getNetworkCountryIso = nullptr;
    
    // SubscriptionInfo methods
    jmethodID sub_getIccId = nullptr;
    jmethodID sub_getCarrierName = nullptr;
    jmethodID sub_getCountryIso = nullptr;
    jmethodID sub_getMcc = nullptr;
    jmethodID sub_getMnc = nullptr;
    jmethodID sub_getCarrierId = nullptr;
    jmethodID sub_getMccString = nullptr;
    jmethodID sub_getMncString = nullptr;

    // Class references
    jclass buildClass = nullptr;

    static int (*original_system_property_get)(const char*, char*);
    
    // Extended property map: config key -> system property name
    static const std::map<std::string, std::string> KEY_TO_PROP;

    // Current spoof config for hooks
    static thread_local AppSpoofConfig tls_config;
    static thread_local bool tls_has_config;
};

// Thread-local storage
extern thread_local std::unordered_map<std::string, std::string> thread_local_spoof_properties;
