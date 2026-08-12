#include <cstring>
#include <string>
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <errno.h>
#include <android/log.h>
#include "zygisk.hpp"

#define TAG "UniversalHide"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

using zygisk::Api;
using zygisk::AppSpecializeArgs;
using zygisk::ServerSpecializeArgs;

static const char* SPOOF_FP   = "google/caiman/caiman:14/AD1A.240905.004/12196292:user/release-keys";
static const char* SPOOF_TAGS = "release-keys";
static const char* SPOOF_TYPE = "user";

static const char* TARGET_PKG[] = {
    "com.google.android.gms",
    "com.android.vending",
    "com.netflix.mediaclient",
    "com.samsung.android.samsungpay",
    "com.paypal.android.p2pmobile",
    "com.whatsapp",
    nullptr
};

static int (*orig_sysprop_get)(const char*, char*) = nullptr;

static bool isTarget(const char* pkg) {
    if (!pkg) return false;
    for (int i = 0; TARGET_PKG[i]; i++)
        if (strstr(pkg, TARGET_PKG[i])) return true;
    return false;
}

static int hook_sysprop_get(const char* name, char* value) {
    int ret = orig_sysprop_get(name, value);
    if (!name || !value) return ret;
    if (!strcmp(name, "ro.boot.verifiedbootstate"))  { strcpy(value, "green");    return strlen(value); }
    if (!strcmp(name, "ro.boot.flash.locked"))        { strcpy(value, "1");        return 1; }
    if (!strcmp(name, "ro.boot.vbmeta.device_state")) { strcpy(value, "locked");   return strlen(value); }
    if (!strcmp(name, "ro.build.tags"))               { strcpy(value, SPOOF_TAGS); return strlen(value); }
    if (!strcmp(name, "ro.build.type"))               { strcpy(value, SPOOF_TYPE); return strlen(value); }
    if (!strcmp(name, "ro.build.fingerprint"))        { strcpy(value, SPOOF_FP);   return strlen(value); }
    if (!strcmp(name, "ro.debuggable"))               { strcpy(value, "0");        return 1; }
    if (!strcmp(name, "ro.secure"))                   { strcpy(value, "1");        return 1; }
    if (!strcmp(name, "service.adb.root"))            { strcpy(value, "0");        return 1; }
    return ret;
}

static void installHooks() {
    void* libc = dlopen("libc.so", RTLD_NOW);
    if (!libc) { LOGE("dlopen libc failed"); return; }
    void* sym = dlsym(libc, "__system_property_get");
    if (sym) {
        orig_sysprop_get = reinterpret_cast<int(*)(const char*,char*)>(sym);
        LOGI("Hooked __system_property_get @ %p", sym);
    }
    dlclose(libc);
}

class UniversalHide : public zygisk::ModuleBase {
public:
    void onLoad(Api* api, JNIEnv* env) override {
        this->api = api;
        this->env = env;
        LOGI("UniversalHide v3.1 loaded");
    }
    void preAppSpecialize(AppSpecializeArgs* args) override {
        const char* name = args->nice_name
            ? env->GetStringUTFChars(args->nice_name, nullptr) : nullptr;
        if (name && isTarget(name)) {
            LOGI("Hooking for: %s", name);
            installHooks();
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
        }
        if (name) env->ReleaseStringUTFChars(args->nice_name, name);
    }
    void postAppSpecialize(const AppSpecializeArgs*) override {}
    void preServerSpecialize(ServerSpecializeArgs*) override {
        api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
    }
private:
    Api*    api = nullptr;
    JNIEnv* env = nullptr;
};

REGISTER_ZYGISK_MODULE(UniversalHide)
