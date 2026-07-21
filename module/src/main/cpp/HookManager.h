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
    ~HookManager();

    void applyHooks(JNIEnv* env, const AppSpoofConfig& config);

private:
    void spoofBuildFields(JNIEnv* env, const AppSpoofConfig& cfg);
    void installPropertyHooks(const AppSpoofConfig& cfg);
    void spoofTelephony(JNIEnv* env, const AppSpoofConfig& cfg);
    void spoofSubscriptionInfo(JNIEnv* env, const AppSpoofConfig& cfg);
    void spoofTelephonyManager(JNIEnv* env, const AppSpoofConfig& cfg);
    void spoofLocation(JNIEnv* env, const AppSpoofConfig& cfg);
    void spoofCountryDetector(JNIEnv* env, const AppSpoofConfig& cfg);
    void spoofNetwork(JNIEnv* env, const AppSpoofConfig& cfg);
    void spoofProcFiles(const AppSpoofConfig& cfg);
    void ensure_libc_info();
    
    static int hooked_system_property_get(const char* name, char* value);
    static int hooked_system_property_read(const prop_info* pi, 
        void (*callback)(void* cookie, const char* name, const char* value, uint32_t serial), 
        void* cookie);

    template<typename Func>
    bool installInlineHook(void* target, Func hook, Func* original);

    zygisk::Api* api;
    dev_t libc_dev = 0;
    ino_t libc_ino = 0;

    struct BuildFields {
        jfieldID BRAND, MODEL, MANUFACTURER, DEVICE, PRODUCT, BOARD, HARDWARE;
        jfieldID FINGERPRINT, ID, DISPLAY, INCREMENTAL, HOST, USER, TAGS;
        jfieldID BOOTLOADER, SERIAL;
    } buildFields;

    jmethodID getSimSerialNumber = nullptr, getDeviceId = nullptr;
    jmethodID getSubscriberId = nullptr, getNetworkOperator = nullptr;
    jmethodID getNetworkOperatorName = nullptr, getSimOperator = nullptr;
    jmethodID getSimOperatorName = nullptr, getSimCountryIso = nullptr;
    jmethodID getNetworkCountryIso = nullptr;
    jmethodID sub_getIccId = nullptr, sub_getCarrierName = nullptr;
    jmethodID sub_getCountryIso = nullptr, sub_getMcc = nullptr;
    jmethodID sub_getMnc = nullptr, sub_getCarrierId = nullptr;
    jmethodID sub_getMccString = nullptr, sub_getMncString = nullptr;

    jclass buildClass = nullptr, telephonyManagerClass = nullptr;
    jclass subscriptionInfoClass = nullptr, wifiInfoClass = nullptr;
    jclass locationClass = nullptr;

    static int (*original_system_property_get)(const char*, char*);
    static int (*original_system_property_read)(const prop_info*, 
        void (*)(void*, const char*, const char*, uint32_t), void*);
    static const std::map<std::string, std::string> KEY_TO_PROP;
    static thread_local AppSpoofConfig tls_config;
    static thread_local bool tls_has_config;
};

extern thread_local std::unordered_map<std::string, std::string> thread_local_spoof_properties;
