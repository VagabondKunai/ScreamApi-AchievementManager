#pragma once
#include "pch.h"
#include "eos-sdk/eos_version.h"
#include "Logger.h"

/**
 * EOS SDK Version Detection and Compatibility Layer
 * 
 * This module detects the game's EOS SDK version at runtime and provides
 * version-aware wrappers for structures that changed between SDK versions.
 */

namespace EOS_Compat {

// Detected SDK version
struct SDKVersion {
	int major = 0;
	int minor = 0;
	int patch = 0;
	int hotfix = 0;
	bool detected = false;
};

extern SDKVersion gameSDKVersion;

/**
 * Detect the EOS SDK version used by the game's DLL
 * @param eosDLL Handle to the loaded EOS SDK DLL
 * @return true if successfully detected
 */
bool detectSDKVersion(HMODULE eosDLL);

/**
 * Get a human-readable version string
 */
const char* getVersionString();

/**
 * Check if a specific SDK version or newer is available
 */
bool isVersionOrNewer(int major, int minor, int patch = 0);

/**
 * Get the appropriate API version constant based on detected SDK version
 * @param apiName Name of the API (e.g. "QueryPlayerAchievements", "PlatformOptions")
 * @return Appropriate API_LATEST value, or -1 if unknown
 */
int getApiVersion(const char* apiName);

/**
 * Check if a specific EOS feature/function is available in the detected SDK
 */
bool isFeatureAvailable(const char* featureName);

/**
 * Log the detected SDK version and available features
 */
void logCompatibilityInfo();

} // namespace EOS_Compat
