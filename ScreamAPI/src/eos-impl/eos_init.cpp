#include "pch.h"
#include "eos-sdk/eos_init.h"
#include "eos-sdk/eos_types.h"
#include "ScreamAPI.h"
#include "achievement_manager.h"
#include "util.h"

// Status enum definitions for v1.15+ mandatory platform initialization
// (these enums don't exist in v1.13.0 SDK, only in v1.15+)
enum EOS_EApplicationStatus
{
	EOS_AS_Foreground = 0,
	EOS_AS_Background = 1
};

enum EOS_ENetworkStatus
{
	EOS_NS_Offline = 0,
	EOS_NS_Online = 3
};

// Forward declarations for v1.15+ mandatory platform initialization
typedef void(*EOS_Platform_SetApplicationStatus_t)(void* Handle, EOS_EApplicationStatus ApplicationStatus);
typedef void(*EOS_Platform_SetNetworkStatus_t)(void* Handle, EOS_ENetworkStatus NetworkStatus);

EOS_DECLARE_FUNC(EOS_HPlatform) EOS_Platform_Create(const EOS_Platform_Options* Options){
	Logger::info("EOS_Platform_Create called - setting hPlatform");
	Logger::debug("EOS_Platform_Create: Options pointer: %p", Options);
	if(Options != nullptr) {
		Logger::debug("EOS_Platform_Create: Options->ApiVersion: %d", Options->ApiVersion);
		Logger::debug("EOS_Platform_Create: Options->Flags: %d", Options->Flags);
	}
	
	if(Config::ForceEpicOverlay()){
		// FIXME: This may lead to a crash with future EOS SDK headers or old dlls...
		Logger::debug("[EOS_Platform_Options]");
		Logger::debug("\t""ApiVersion: %d", Options->ApiVersion);
		Logger::debug("\t""Flags: %d", Options->Flags);

		auto mOptions = const_cast<EOS_Platform_Options*>(Options);
		mOptions->Flags = 0;
	}

	static auto proxy = ScreamAPI::proxyFunction(&EOS_Platform_Create, __func__);
	auto result = proxy(Options);

	// Save the EOS_HPlatform handle for later use
	Util::hPlatform = result;
	Logger::info("EOS_Platform_Create result: %p", result);
	
	if(result == nullptr) {
		Logger::error("EOS_Platform_Create returned NULL - initialization will fail!");
		return result;
	}

	// Add mandatory v1.15+ platform status initialization calls
	// These are required for proper EOS SDK operation in v1.15.0 and later
	if(ScreamAPI::originalDLL != nullptr) {
		Logger::info("EOS_Platform_Create: Calling SetApplicationStatus and SetNetworkStatus (v1.15+ mandatory)");
		
		// Set application status to foreground
		EOS_Platform_SetApplicationStatus_t SetAppStatusFunc = 
			(EOS_Platform_SetApplicationStatus_t)GetProcAddress(
				(HMODULE)ScreamAPI::originalDLL, "_EOS_Platform_SetApplicationStatus@8");
		
		if(SetAppStatusFunc != nullptr) {
			try {
				SetAppStatusFunc(result, EOS_AS_Foreground);
				Logger::debug("Successfully called EOS_Platform_SetApplicationStatus");
			} catch(...) {
				Logger::warn("Exception calling EOS_Platform_SetApplicationStatus");
			}
		} else {
			Logger::debug("EOS_Platform_SetApplicationStatus not found (older SDK version)");
		}

		// Set network status to online
		EOS_Platform_SetNetworkStatus_t SetNetStatusFunc = 
			(EOS_Platform_SetNetworkStatus_t)GetProcAddress(
				(HMODULE)ScreamAPI::originalDLL, "_EOS_Platform_SetNetworkStatus@8");
		
		if(SetNetStatusFunc != nullptr) {
			try {
				SetNetStatusFunc(result, EOS_NS_Online);
				Logger::debug("Successfully called EOS_Platform_SetNetworkStatus");
			} catch(...) {
				Logger::warn("Exception calling EOS_Platform_SetNetworkStatus");
			}
		} else {
			Logger::debug("EOS_Platform_SetNetworkStatus not found (older SDK version)");
		}
	}

	return result;
}
