#include "pch.h"
#include "eos-sdk/eos_init.h"
#include "eos-sdk/eos_types.h"
#include "ScreamAPI.h"
#include "achievement_manager.h"
#include "util.h"

// v1.17.3 SDK already has these enums in eos_types.h, so we don't redefine them
// EOS_EApplicationStatus and EOS_ENetworkStatus are now part of the official SDK

EOS_DECLARE_FUNC(EOS_HPlatform) EOS_Platform_Create(const EOS_Platform_Options* Options){
	Logger::info("EOS_Platform_Create called - setting hPlatform");
	Logger::debug("EOS_Platform_Create: Options pointer: %p", Options);
	if(Options != nullptr) {
		Logger::debug("EOS_Platform_Create: Options->ApiVersion: %d", Options->ApiVersion);
		Logger::debug("EOS_Platform_Create: Options->Flags: %llu", Options->Flags);
	}
	
	if(Config::ForceEpicOverlay()){
		// FIXME: This may lead to a crash with future EOS SDK headers or old dlls...
		Logger::debug("[EOS_Platform_Options]");
		Logger::debug("\t""ApiVersion: %d", Options->ApiVersion);
		Logger::debug("\t""Flags: %llu", Options->Flags);

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

	// Platform created successfully - initialize achievement manager now
	Logger::info("EOS Platform successfully created by game - initializing achievement manager");
	
	// Add mandatory v1.15+ platform status initialization calls
	// These are required for proper EOS SDK operation in v1.15.0 and later
	if(ScreamAPI::originalDLL != nullptr) {
		Logger::debug("EOS_Platform_Create: Calling SetApplicationStatus and SetNetworkStatus (v1.15+ mandatory)");
		
		// Set application status to foreground
		typedef void (*EOS_Platform_SetApplicationStatus_t)(EOS_HPlatform, EOS_EApplicationStatus);
		EOS_Platform_SetApplicationStatus_t SetAppStatusFunc = 
			(EOS_Platform_SetApplicationStatus_t)GetProcAddress(
				(HMODULE)ScreamAPI::originalDLL, "EOS_Platform_SetApplicationStatus");
		
		if(SetAppStatusFunc != nullptr) {
			try {
				SetAppStatusFunc(result, EOS_EApplicationStatus::EOS_AS_BackgroundConstrained);
				Logger::debug("Successfully called EOS_Platform_SetApplicationStatus");
			} catch(...) {
				Logger::warn("Exception calling EOS_Platform_SetApplicationStatus");
			}
		} else {
			Logger::debug("EOS_Platform_SetApplicationStatus not found (older SDK version)");
		}

		// Set network status to online
		typedef void (*EOS_Platform_SetNetworkStatus_t)(EOS_HPlatform, EOS_ENetworkStatus);
		EOS_Platform_SetNetworkStatus_t SetNetStatusFunc = 
			(EOS_Platform_SetNetworkStatus_t)GetProcAddress(
				(HMODULE)ScreamAPI::originalDLL, "EOS_Platform_SetNetworkStatus");
		
		if(SetNetStatusFunc != nullptr) {
			try {
				SetNetStatusFunc(result, EOS_ENetworkStatus::EOS_NS_Online);
				Logger::debug("Successfully called EOS_Platform_SetNetworkStatus");
			} catch(...) {
				Logger::warn("Exception calling EOS_Platform_SetNetworkStatus");
			}
		} else {
			Logger::debug("EOS_Platform_SetNetworkStatus not found (older SDK version)");
		}
	}
	
	// Trigger achievement manager initialization now that platform is ready
	if(Config::EnableOverlay()) {
		std::thread([]() {
			Sleep(500);  // Small delay to ensure platform is fully initialized
			Logger::info("[INIT] Platform detected - triggering achievement manager initialization");
			AchievementManager::init();
		}).detach();
	}

	return result;
}
