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

    zygisk::Api* api;
    dev_t libc_dev = 0;
    ino_t libc_ino = 0;

    struct BuildFields {
        jfieldID BRAND, MODEL, MANUFACTURER, DEVICE, PRODUCT, BOARD, HARDWARE;
        jfieldID FINGERPRINT, ID, DISPLAY, INCREMENTAL, HOST, USER, TAGS;
        jfieldID BOOTLOADER;
        jfieldID SERIAL;
    } buildFields;

    jclass buildClass = nullptr;

    static int (*original_system_property_get)(const char*, char*);
    static const std::map<std::string, std::string> KEY_TO_PROP;
    static thread_local AppSpoofConfig tls_config;
    static thread_local bool tls_has_config;
};

extern thread_local std::unordered_map<std::string, std::string> thread_local_spoof_properties;
