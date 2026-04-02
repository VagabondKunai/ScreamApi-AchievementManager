#include "pch.h"
#include "ScreamAPI.h"
#include "constants.h"
#include "util.h"
#include <Overlay.h>
#include "achievement_manager.h"
#include "eos_compat.h"
#include "eos_hooks.h"
#include "eos-sdk/eos_init.h"
#include "eos-sdk/eos_types.h"
#include <future>
#include <vector>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

using namespace Util;

// ------------------------------------------------------------------
// Recursively search for the EOS SDK DLL starting from a root folder
// ------------------------------------------------------------------
static HMODULE FindEOSSDKRecursive(const fs::path& root, const std::wstring& dllName, int maxDepth = 8) {
    if (!fs::exists(root) || !fs::is_directory(root))
        return nullptr;

    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); ++it) {
        // Depth limit
        if (it.depth() > maxDepth)
            continue;

        const auto& entry = *it;
        if (!entry.is_regular_file())
            continue;

        if (entry.path().filename() == dllName) {
            HMODULE h = LoadLibrary(entry.path().c_str());
            if (h) {
                Logger::info("Found original EOS SDK in: %ls", entry.path().c_str());
                return h;
            }
        }
    }
    return nullptr;
}

// ------------------------------------------------------------------
// Alternative: Win32 FindFirstFile implementation (no C++17 dependency)
// (kept as fallback if filesystem is problematic)
// ------------------------------------------------------------------
static HMODULE FindEOSSDKRecursiveWin32(const std::wstring& root, const std::wstring& dllName, int maxDepth = 8) {
    // Implementation using FindFirstFile / FindNextFile
    // ... (omitted for brevity, we'll use the C++17 version above)
    return nullptr;
}

namespace ScreamAPI
{
    HMODULE thisDLL = nullptr;
    HMODULE originalDLL = nullptr;
    bool isScreamAPIinitialized = false;

    void init(HMODULE hModule) {
        if (isScreamAPIinitialized)
            return;

        isScreamAPIinitialized = true;
        thisDLL = hModule;

        const auto iniPath = getDLLparentDir(hModule) / SCREAM_API_CONFIG;
        Config::init(iniPath.generic_wstring());

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

        std::string origDllA(SCREAM_API_ORIG_DLL);
        std::wstring origDllW(origDllA.begin(), origDllA.end());

        // -----------------------------------------------------------------
        // 1. Try to get the already-loaded module (injection method)
        //    Retry for up to 10 seconds (100 * 100ms)
        // -----------------------------------------------------------------
        HMODULE original = nullptr;
        for (int i = 0; i < 100; ++i) {
            original = GetModuleHandle(origDllW.c_str());
            if (original) {
                Logger::debug("Got handle to already-loaded EOS SDK after %d ms", i * 100);
                break;
            }
            Sleep(100);
        }

        // -----------------------------------------------------------------
        // 2. If not found, try to load it from the current directory
        // -----------------------------------------------------------------
        if (!original) {
            Logger::debug("GetModuleHandle failed, trying LoadLibrary from current directory");
            const auto originalDllPath = getDLLparentDir(hModule) / origDllW;
            original = LoadLibrary(originalDllPath.c_str());
        }

        // -----------------------------------------------------------------
        // 3. Recursive search (C++17) – finds SDK buried in subfolders
        // -----------------------------------------------------------------
        if (!original) {
            Logger::debug("Searching recursively for %ls in game directory...", origDllW.c_str());
            const auto gameDir = getDLLparentDir(hModule);
            original = FindEOSSDKRecursive(gameDir, origDllW, 6);
        }

        // -----------------------------------------------------------------
        // 4. Fallback: known subfolders (kept for compatibility)
        // -----------------------------------------------------------------
        if (!original) {
            const std::vector<std::wstring> subfolders = {
                L"Engine/Binaries/ThirdParty/EOS/Win32/",
                L"Engine/Binaries/ThirdParty/EOS/Win64/",
                L"Binaries/Win32/",
                L"Binaries/Win64/",
                L"Beholder_Data/Plugins/",
                L"Beholder_Data/Plugins/x86/",
                L"Beholder_Data/Plugins/x86_64/",
            };
            for (const auto& sub : subfolders) {
                auto path = getDLLparentDir(hModule) / sub / origDllW;
                HMODULE temp = LoadLibrary(path.c_str());
                if (temp) {
                    original = temp;
                    Logger::info("Found original EOS SDK in: %ls", path.c_str());
                    break;
                }
            }
        }

        // -----------------------------------------------------------------
        // 5. Last resort: Unity _Data/Plugins folder (already covered by recursive)
        //    but kept for safety.
        // -----------------------------------------------------------------
        if (!original) {
            // original = FindInUnityDataPlugins(...); // covered by recursive, skip
        }

        if (original) {
            originalDLL = original;
            Logger::info("Successfully obtained original EOS SDK: %s", SCREAM_API_ORIG_DLL);

            if (EOS_Compat::detectSDKVersion((HMODULE)originalDLL)) {
                EOS_Compat::logCompatibilityInfo();
            } else {
                Logger::error("Failed to detect game's EOS SDK version");
            }

            Logger::debug("Calling EOS_Hooks::InitializeHooks...");
            if (EOS_Hooks::InitializeHooks((HMODULE)originalDLL)) {
                Logger::info("MinHook hooking system initialized - all EOS functions hooked");
            } else {
                Logger::error("Failed to initialize MinHook hooking system!");
            }
        } else {
            Logger::error("Failed to locate original EOS SDK: %s", SCREAM_API_ORIG_DLL);
            Logger::error("Make sure the game has loaded the EOS SDK (or use proxy method)");
        }

        Logger::debug("DLL init complete");
        Logger::info("Waiting for game to create EOS Platform via EOS_Platform_Create hook");

        if (Config::EnableOverlay()) {
            Logger::debug("Scheduling overlay initialization with platform polling");
            static auto deferredInit = std::async(std::launch::async, [hModule]() {
                const int MAX_WAIT_SECONDS = 60;
                const int POLL_INTERVAL_MS = 1000;
                int elapsedSeconds = 0;

                Logger::info("Waiting for game to initialize EOS platform...");

                while (elapsedSeconds < MAX_WAIT_SECONDS) {
                    Sleep(POLL_INTERVAL_MS);
                    elapsedSeconds++;

                    if (Util::isEOSPlatformReady()) {
                        Logger::info("EOS Platform detected as ready after %d seconds", elapsedSeconds);
                        Logger::info("Initializing overlay and achievement manager");
                        AchievementManager::initWithOverlay(hModule);
                        return;
                    }

                    if (elapsedSeconds % 10 == 0) {
                        Logger::debug("Still waiting for EOS platform... (%d/%d seconds)",
                                      elapsedSeconds, MAX_WAIT_SECONDS);
                    }
                }

                Logger::error("Timed out waiting for EOS platform after %d seconds", MAX_WAIT_SECONDS);
                Logger::error("Achievement overlay will not be available");
                Logger::warn("The game may not use EOS Achievements or requires manual initialization");
            });
        }
    }

    void destroy() {
        Logger::info("Game requested to shutdown the EOS SDK");

        EOS_Hooks::ShutdownHooks();

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