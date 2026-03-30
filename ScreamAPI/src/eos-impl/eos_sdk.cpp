#include "pch.h"
#include "eos-sdk/eos_sdk.h"
#include "ScreamAPI.h"
#include "Logger.h"
#include "util.h"   // for Util::hPlatform

static void setPlatformStatus(EOS_HPlatform platform) {
    if (!platform) return;

    typedef void (EOS_CALL *SetAppStatus_t)(EOS_HPlatform, int);
    typedef void (EOS_CALL *SetNetStatus_t)(EOS_HPlatform, int);

    static SetAppStatus_t setAppStatus = nullptr;
    static SetNetStatus_t setNetStatus = nullptr;
    static bool attempted = false;

    if (!attempted) {
        attempted = true;
#ifdef _WIN64
        setAppStatus = (SetAppStatus_t)GetProcAddress(ScreamAPI::originalDLL, "EOS_Platform_SetApplicationStatus");
        setNetStatus = (SetNetStatus_t)GetProcAddress(ScreamAPI::originalDLL, "EOS_Platform_SetNetworkStatus");
#else
        setAppStatus = (SetAppStatus_t)GetProcAddress(ScreamAPI::originalDLL, "_EOS_Platform_SetApplicationStatus@8");
        setNetStatus = (SetNetStatus_t)GetProcAddress(ScreamAPI::originalDLL, "_EOS_Platform_SetNetworkStatus@8");
#endif
    }

    if (setAppStatus) {
        setAppStatus(platform, 0); // EOS_AS_Visible
        Logger::debug("SetApplicationStatus called");
    } else {
        Logger::debug("SetApplicationStatus not available");
    }
    if (setNetStatus) {
        setNetStatus(platform, 3); // EOS_NS_Online
        Logger::debug("SetNetworkStatus called");
    } else {
        Logger::debug("SetNetworkStatus not available");
    }
}

EOS_DECLARE_FUNC(EOS_HAuth) EOS_Platform_GetAuthInterface(EOS_HPlatform Handle){
    Logger::debug("GetAuthInterface called with Handle=%p", Handle);
    if (Handle && !ScreamAPI::hPlatform) {
        ScreamAPI::hPlatform = Handle;
        Util::hPlatform = Handle;
        Logger::debug("Captured platform handle from GetAuthInterface: %p", Handle);
        setPlatformStatus(Handle);
    }
    EOS_IMPLEMENT_FUNC(EOS_Platform_GetAuthInterface, Handle);
}

EOS_DECLARE_FUNC(EOS_HAchievements) EOS_Platform_GetAchievementsInterface(EOS_HPlatform Handle){
    Logger::debug("GetAchievementsInterface called with Handle=%p", Handle);
    if (Handle && !ScreamAPI::hPlatform) {
        ScreamAPI::hPlatform = Handle;
        Util::hPlatform = Handle;
        Logger::debug("Captured platform handle from GetAchievementsInterface: %p", Handle);
        setPlatformStatus(Handle);
    }
    EOS_IMPLEMENT_FUNC(EOS_Platform_GetAchievementsInterface, Handle);
}

EOS_DECLARE_FUNC(EOS_HConnect) EOS_Platform_GetConnectInterface(EOS_HPlatform Handle){
    Logger::debug("GetConnectInterface called with Handle=%p", Handle);
    if (Handle && !ScreamAPI::hPlatform) {
        ScreamAPI::hPlatform = Handle;
        Util::hPlatform = Handle;
        Logger::debug("Captured platform handle from GetConnectInterface: %p", Handle);
        setPlatformStatus(Handle);
    }
    EOS_IMPLEMENT_FUNC(EOS_Platform_GetConnectInterface, Handle);
}