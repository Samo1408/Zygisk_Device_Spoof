/* Zygisk Device Spoof Module - Extended Edition
 * Fork of original by Qjj0204
 * Enhanced with comprehensive device/SIM/telephony/country spoofing
 * WebUI support for KernelSU
 */

#include <memory>
#include <string_view>
#include <unistd.h>

#include "zygisk.hpp"
#include "ConfigManager.h"
#include "HookManager.h"

#ifdef DEBUG
#include <android/log.h>
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "ZygiskDeviceSpoof", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "ZygiskDeviceSpoof", __VA_ARGS__)
#else
#define LOGD(...)
#define LOGE(...)
#endif

using zygisk::Api;
using zygisk::AppSpecializeArgs;
using zygisk::ServerSpecializeArgs;

// Forward declaration
static void companion_handler(int socket_fd);

class SpoofModule : public zygisk::ModuleBase {
public:
    void onLoad(Api* api, JNIEnv* env) override {
        this->api = api;
        this->env = env;
        LOGD("Module loaded");
    }

    void preAppSpecialize(AppSpecializeArgs* args) override {
        auto* raw_app_name = env->GetStringUTFChars(args->nice_name, nullptr);
        if (!raw_app_name) return;
        std::string app_name(raw_app_name);
        env->ReleaseStringUTFChars(args->nice_name, raw_app_name);

        LOGD("preAppSpecialize for: %s", app_name.c_str());

        // Connect to companion for config
        int fd = api->connectCompanion();
        if (fd < 0) {
            LOGD("Failed to connect to companion for %s", app_name.c_str());
            api->setOption(zygisk::Option::FORCE_DENYLIST_UNMOUNT);
            return;
        }

        // Send app name
        write(fd, app_name.c_str(), app_name.length() + 1);

        // Read serialized config from companion
        std::string cfg_buffer;
        char chunk[4096];
        ssize_t bytes_read;
        while ((bytes_read = read(fd, chunk, sizeof(chunk))) > 0) {
            cfg_buffer.append(chunk, bytes_read);
        }
        close(fd);

        if (bytes_read < 0) {
            LOGE("Failed to read from companion for %s", app_name.c_str());
            api->setOption(zygisk::Option::FORCE_DENYLIST_UNMOUNT);
            return;
        }

        if (cfg_buffer.empty()) {
            LOGD("No config for %s, skipping", app_name.c_str());
            api->setOption(zygisk::Option::FORCE_DENYLIST_UNMOUNT);
            return;
        }

        // Deserialize config
        AppSpoofConfig cfg;
        deserializeConfig(cfg_buffer, cfg);

        if (!cfg.enabled) {
            api->setOption(zygisk::Option::FORCE_DENYLIST_UNMOUNT);
            return;
        }

        // Apply hooks
        if (!hookManager) {
            hookManager = std::make_unique<HookManager>(api);
        }
        hookManager->applyHooks(env, cfg);

        // Force unmount for anti-detection
        api->setOption(zygisk::Option::FORCE_DENYLIST_UNMOUNT);
    }

private:
    Api* api = nullptr;
    JNIEnv* env = nullptr;
    std::unique_ptr<HookManager> hookManager;

    // Deserialize config from companion (simple binary protocol)
    void deserializeConfig(const std::string& buf, AppSpoofConfig& cfg) {
        const char* data = buf.data();
        size_t len = buf.size();
        size_t pos = 0;

        auto read_str = [&]() -> std::string {
            if (pos >= len) return "";
            uint16_t slen;
            memcpy(&slen, data + pos, 2); pos += 2;
            if (pos + slen > len) return "";
            std::string s(data + pos, slen); pos += slen;
            return s;
        };

        auto read_int = [&]() -> int {
            if (pos + 4 > len) return -1;
            int v; memcpy(&v, data + pos, 4); pos += 4;
            return v;
        };

        auto read_double = [&]() -> double {
            if (pos + 8 > len) return 0.0;
            double v; memcpy(&v, data + pos, 8); pos += 8;
            return v;
        };

        auto read_bool = [&]() -> bool {
            if (pos >= len) return false;
            return data[pos++] == 1;
        };

        cfg.brand = read_str();
        cfg.model = read_str();
        cfg.manufacturer = read_str();
        cfg.device = read_str();
        cfg.product = read_str();
        cfg.board = read_str();
        cfg.hardware = read_str();
        cfg.buildFingerprint = read_str();
        cfg.buildId = read_str();
        cfg.buildDisplayId = read_str();
        cfg.buildIncremental = read_str();
        cfg.securityPatch = read_str();
        cfg.buildDescription = read_str();
        cfg.buildFlavor = read_str();
        cfg.buildProduct = read_str();
        cfg.bootloader = read_str();
        cfg.baseband = read_str();
        cfg.buildHost = read_str();
        cfg.buildUser = read_str();
        cfg.buildTags = read_str();
        cfg.socModel = read_str();
        cfg.socManufacturer = read_str();
        cfg.iccOperatorIsoCountry = read_str();
        cfg.iccOperatorNumeric = read_str();
        cfg.operatorIsoCountry = read_str();
        cfg.operatorNumeric = read_str();
        cfg.simSerial = read_str();
        cfg.iccId = read_str();
        cfg.subscriberId = read_str();
        cfg.carrierName = read_str();
        cfg.countryIso = read_str();
        cfg.countryCode = read_str();
        cfg.ipAddress = read_str();
        cfg.wifiSsid = read_str();
        cfg.androidId = read_str();
        cfg.gsfId = read_str();
        cfg.appSetId = read_str();
        cfg.mediaDrmId = read_str();
        cfg.userAgent = read_str();
        cfg.defaultTimezone = read_str();
        cfg.carrierId = read_int();
        cfg.mcc = read_int();
        cfg.mnc = read_int();
        cfg.ramGb = read_int();
        cfg.latitude = read_double();
        cfg.longitude = read_double();
        cfg.enabled = read_bool();
    }
};

// --- Companion Process ---
static void serializeConfig(const AppSpoofConfig& cfg, std::string& out) {
    auto write_str = [&](const std::string& s) {
        uint16_t len = s.length();
        out.append(reinterpret_cast<const char*>(&len), 2);
        out.append(s);
    };
    auto write_int = [&](int v) {
        out.append(reinterpret_cast<const char*>(&v), 4);
    };
    auto write_double = [&](double v) {
        out.append(reinterpret_cast<const char*>(&v), 8);
    };
    auto write_bool = [&](bool v) {
        out.push_back(v ? 1 : 0);
    };

    write_str(cfg.brand);
    write_str(cfg.model);
    write_str(cfg.manufacturer);
    write_str(cfg.device);
    write_str(cfg.product);
    write_str(cfg.board);
    write_str(cfg.hardware);
    write_str(cfg.buildFingerprint);
    write_str(cfg.buildId);
    write_str(cfg.buildDisplayId);
    write_str(cfg.buildIncremental);
    write_str(cfg.securityPatch);
    write_str(cfg.buildDescription);
    write_str(cfg.buildFlavor);
    write_str(cfg.buildProduct);
    write_str(cfg.bootloader);
    write_str(cfg.baseband);
    write_str(cfg.buildHost);
    write_str(cfg.buildUser);
    write_str(cfg.buildTags);
    write_str(cfg.socModel);
    write_str(cfg.socManufacturer);
    write_str(cfg.iccOperatorIsoCountry);
    write_str(cfg.iccOperatorNumeric);
    write_str(cfg.operatorIsoCountry);
    write_str(cfg.operatorNumeric);
    write_str(cfg.simSerial);
    write_str(cfg.iccId);
    write_str(cfg.subscriberId);
    write_str(cfg.carrierName);
    write_str(cfg.countryIso);
    write_str(cfg.countryCode);
    write_str(cfg.ipAddress);
    write_str(cfg.wifiSsid);
    write_str(cfg.androidId);
    write_str(cfg.gsfId);
    write_str(cfg.appSetId);
    write_str(cfg.mediaDrmId);
    write_str(cfg.userAgent);
    write_str(cfg.defaultTimezone);
    write_int(cfg.carrierId);
    write_int(cfg.mcc);
    write_int(cfg.mnc);
    write_int(cfg.ramGb);
    write_double(cfg.latitude);
    write_double(cfg.longitude);
    write_bool(cfg.enabled);
}

static void companion_handler(int socket_fd) {
    static ConfigManager configManager;
    
    char app_name[256] = {};
    ssize_t bytes_read = read(socket_fd, app_name, sizeof(app_name) - 1);
    if (bytes_read <= 0) {
        close(socket_fd);
        return;
    }
    app_name[bytes_read] = '\0';

    configManager.loadOrReloadConfig();

    auto cfg_opt = configManager.getConfigForApp(app_name);
    if (cfg_opt) {
        std::string buffer;
        serializeConfig(*cfg_opt, buffer);
        if (!buffer.empty()) {
            write(socket_fd, buffer.c_str(), buffer.length());
        }
    }

    close(socket_fd);
}

REGISTER_ZYGISK_MODULE(SpoofModule)
REGISTER_ZYGISK_COMPANION(companion_handler)
