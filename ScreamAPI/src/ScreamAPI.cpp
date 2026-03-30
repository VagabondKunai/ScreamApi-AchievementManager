#include "pch.h"
#include "ScreamAPI.h"
#include "constants.h"
#include "util.h"
#include <Overlay.h>
#include "achievement_manager.h"
#include "eos-sdk/eos_init.h"
#include "eos-sdk/eos_types.h"
#include <future>

using namespace Util;

namespace ScreamAPI
{
	// Initialize extern variables
	HMODULE thisDLL = nullptr;
	HMODULE originalDLL = nullptr;
	EOS_HPlatform hPlatform = nullptr;   // <-- ADDED
	bool isScreamAPIinitialized = false;

	void init(HMODULE hModule) {
		// Check if DLL is already initialized
		if (isScreamAPIinitialized) // Do we really need to check that?
			return;

		isScreamAPIinitialized = true;

		thisDLL = hModule;

		// Initialize Config
		const auto iniPath = getDLLparentDir(hModule) / SCREAM_API_CONFIG;
		Config::init(iniPath.generic_wstring());

		// Initialize Logger
		const auto logPath = getDLLparentDir(hModule) / Config::LogFilename();
		Logger::init(Config::EnableLogging(),
		             Config::LogDLCQueries(),
		             Config::LogAchievementQueries(),
		             Config::LogOverlay(),
		             Config::LogLevel(),
		             logPath.generic_wstring());

		Logger::info("ScreamAPI v" SCREAM_API_VERSION);
		Logger::debug("DLL init function called");
		Logger::debug("EnableOverlay: %s", Config::EnableOverlay() ? "true" : "false");

		// Load original library
		const auto originalDllPath = getDLLparentDir(hModule) / SCREAM_API_ORIG_DLL;
		originalDLL = LoadLibrary(originalDllPath.generic_wstring().c_str());
		if (originalDLL)
			Logger::info("Successfully loaded original EOS SDK: %s", SCREAM_API_ORIG_DLL);
		else
			Logger::error("Failed to load original EOS SDK: %s", SCREAM_API_ORIG_DLL);
		
		// Try to initialize the EOS platform if game hasn't called EOS_Platform_Create yet
		// This is critical for v1.15+ SDK compatibility
		if(hPlatform == nullptr && originalDLL != nullptr) {
			Logger::info("Platform not initialized by game - attempting manual initialization");
			
			typedef EOS_HPlatform (*EOS_Platform_Create_t)(const EOS_Platform_Options* Options);
			EOS_Platform_Create_t CreatePlatformFunc = 
				(EOS_Platform_Create_t)GetProcAddress((HMODULE)originalDLL, "_EOS_Platform_Create@4");
			
			if(CreatePlatformFunc != nullptr) {
				Logger::debug("Found EOS_Platform_Create - calling with default options");
				
				// Create platform with minimal options
				EOS_Platform_Options PlatformOptions = {};
				PlatformOptions.ApiVersion = EOS_PLATFORM_OPTIONS_API_LATEST;
				PlatformOptions.Flags = 0;
				
				EOS_HPlatform platform = CreatePlatformFunc(&PlatformOptions);
				if(platform != nullptr) {
					hPlatform = platform;
					Logger::info("Successfully created EOS platform: %p", platform);
					
					// Call mandatory v1.15+ initialization functions for platform status
					typedef void (*EOS_Platform_SetApplicationStatus_t)(void* Handle, int ApplicationStatus);
					typedef void (*EOS_Platform_SetNetworkStatus_t)(void* Handle, int NetworkStatus);
					
					EOS_Platform_SetApplicationStatus_t SetAppStatusFunc = 
						(EOS_Platform_SetApplicationStatus_t)GetProcAddress((HMODULE)originalDLL, "_EOS_Platform_SetApplicationStatus@8");
					if(SetAppStatusFunc != nullptr) {
						SetAppStatusFunc(platform, 0); // EOS_AS_Foreground
						Logger::debug("Successfully called EOS_Platform_SetApplicationStatus");
					} else {
						Logger::debug("EOS_Platform_SetApplicationStatus not found (may be older SDK)");
					}
					
					EOS_Platform_SetNetworkStatus_t SetNetStatusFunc = 
						(EOS_Platform_SetNetworkStatus_t)GetProcAddress((HMODULE)originalDLL, "_EOS_Platform_SetNetworkStatus@8");
					if(SetNetStatusFunc != nullptr) {
						SetNetStatusFunc(platform, 3); // EOS_NS_Online
						Logger::debug("Successfully called EOS_Platform_SetNetworkStatus");
					} else {
						Logger::debug("EOS_Platform_SetNetworkStatus not found (may be older SDK)");
					}
				} else {
					Logger::error("EOS_Platform_Create returned NULL - platform initialization failed");
				}
			} else {
				Logger::warn("EOS_Platform_Create not found in original DLL - using decorated export name");
			}
		}
		
		Logger::debug("DLL init complete");

		// Initialize overlay and achievement manager with a delay to allow game authentication
		if (Config::EnableOverlay()) {
			Logger::debug("Scheduling deferred overlay initialization");
			static auto deferredInit = std::async(std::launch::async, [hModule]() {
				Sleep(10000);  // Wait 10 seconds for game to authenticate
				Logger::debug("Executing deferred overlay initialization");
				AchievementManager::initWithOverlay(hModule);
			});
		}
	}


	void destroy() {
		Logger::info("Game requested to shutdown the EOS SDK");
		if (Config::EnableOverlay()) {
			Logger::info("Shutting down overlay");
			Overlay::Shutdown();
			Logger::info("Overlay shutdown complete");
		}
		Logger::info("Freeing original DLL");
		FreeLibrary(originalDLL);
		Logger::flush();
		isScreamAPIinitialized = false;
	}
}