// ============================================================================
// PATCHED CODE SNIPPETS - Ready to Copy/Paste
// ScreamAPI Achievement Manager - EOS SDK v1.13 → v1.16.3 Compatibility
// ============================================================================

// ============================================================================
// PATCH 1: achievement_manager.cpp - Line ~288-298
// Function: queryDefinitionsComplete() - Near the end
// ============================================================================

// ORIGINAL CODE (BROKEN):
/*
void EOS_CALL queryDefinitionsComplete(const EOS_Achievements_OnQueryDefinitionsCompleteCallbackInfo* Data){
    // ... existing code ...
    
    // Query player achievements to update the ones that are already unlocked
    EOS_Achievements_QueryPlayerAchievementsOptions QueryAchievementsOptions = {
            EOS_ACHIEVEMENTS_QUERYPLAYERACHIEVEMENTS_API_LATEST,
            getProductUserId()
    };

    EOS_Achievements_QueryPlayerAchievements(
        getHAchievements(),
        &QueryAchievementsOptions,
        nullptr,
        queryPlayerAchievementsComplete
    );
}
*/

// FIXED CODE (COPY THIS):
void EOS_CALL queryDefinitionsComplete(const EOS_Achievements_OnQueryDefinitionsCompleteCallbackInfo* Data){
    Logger::debug("[ACH] queryDefinitionsComplete called with ResultCode: %d", Data->ResultCode);

    // Abort if something went wrong
    if(Data->ResultCode != EOS_EResult::EOS_Success){
        Logger::error("[ACH] Failed to query achievement definitions. Result: %s",
            EOS_EResult_ToString(Data->ResultCode));
        
        // If we got InvalidParameters, the user IDs might not be ready yet - retry
        if(Data->ResultCode == EOS_EResult::EOS_InvalidParameters) {
            static int retryCount = 0;
            if(retryCount < 10) {
                retryCount++;
                Logger::warn("[ACH] InvalidParameters error - ProductUserId or EpicAccountId may be NULL");
                Logger::debug("[ACH]   Retrying achievement query (%d/10) in 2 seconds", retryCount);
                Logger::debug("[ACH]   ProductUserId: %p", getProductUserId());
                Logger::debug("[ACH]   EpicAccountId: %p", getEpicAccountId());
                static auto retryJob = std::async(std::launch::async, []() {
                    Sleep(2000);
                    queryAchievementDefinitions();
                });
            } else {
                Logger::error("[ACH] Max retries (10) exceeded. Giving up on achievement query.");
            }
        }
        return;
    }

    Logger::debug("[ACH] Achievement definitions query succeeded");

    // Get number of achievement definitions
    static EOS_Achievements_GetAchievementDefinitionCountOptions GetCountOptions{
        EOS_ACHIEVEMENTS_GETACHIEVEMENTDEFINITIONCOUNT_API_LATEST
    };
    auto achievementDefinitionCount = EOS_Achievements_GetAchievementDefinitionCount(getHAchievements(), &GetCountOptions);
    
    // ... (your existing achievement definition iteration code stays the same) ...

    // Query player achievements to update the ones that are already unlocked
    // *** CRITICAL FIX: Added LocalUserId field ***
    EOS_Achievements_QueryPlayerAchievementsOptions QueryAchievementsOptions = {
            EOS_ACHIEVEMENTS_QUERYPLAYERACHIEVEMENTS_API_LATEST,  // ApiVersion
            getProductUserId(),                                   // TargetUserId
            getProductUserId()                                    // LocalUserId - FIXED: Was missing!
    };

    EOS_Achievements_QueryPlayerAchievements(
        getHAchievements(),
        &QueryAchievementsOptions,
        nullptr,
        queryPlayerAchievementsComplete
    );
}


// ============================================================================
// PATCH 2: achievement_manager.cpp - Line ~144-159
// Function: queryPlayerAchievementsComplete()
// ============================================================================

// ORIGINAL CODE (BROKEN):
/*
void EOS_CALL queryPlayerAchievementsComplete(const EOS_Achievements_OnQueryPlayerAchievementsCompleteCallbackInfo* Data){
    // ... validation code ...
    
    for(unsigned int i = 0; i < playerAchievementsCount; i++){
        EOS_Achievements_CopyPlayerAchievementByIndexOptions CopyAchievementOptions{
            EOS_ACHIEVEMENTS_COPYPLAYERACHIEVEMENTBYINDEX_API_LATEST,
            getProductUserId(),
            i
        };
        // ... rest of code ...
    }
}
*/

// FIXED CODE (COPY THIS):
void EOS_CALL queryPlayerAchievementsComplete(const EOS_Achievements_OnQueryPlayerAchievementsCompleteCallbackInfo* Data){
    Logger::debug("queryPlayerAchievementsComplete");

    // Abort if something went wrong
    if(Data->ResultCode != EOS_EResult::EOS_Success){
        Logger::error("Failed to query player achievements. Result string: %s",
            EOS_EResult_ToString(Data->ResultCode));
        // Still initialize overlay even if query failed - achievements will show as locked
        Overlay::Init(ScreamAPI::thisDLL, &achievements, unlockAchievement);
        return;
    }

    // Get number of player achievements
    static EOS_Achievements_GetPlayerAchievementCountOptions GetCountOptions{
        EOS_ACHIEVEMENTS_GETPLAYERACHIEVEMENTCOUNT_API_LATEST,
        getProductUserId()
    };
    auto playerAchievementsCount = EOS_Achievements_GetPlayerAchievementCount(getHAchievements(), &GetCountOptions);
    
    // Iterate over queried player achievements and update our own structs
    for(unsigned int i = 0; i < playerAchievementsCount; i++){
        // *** CRITICAL FIX: Added LocalUserId field ***
        EOS_Achievements_CopyPlayerAchievementByIndexOptions CopyAchievementOptions{
            EOS_ACHIEVEMENTS_COPYPLAYERACHIEVEMENTBYINDEX_API_LATEST,  // ApiVersion
            getProductUserId(),                                        // TargetUserId
            i,                                                         // AchievementIndex
            getProductUserId()                                         // LocalUserId - FIXED: Was missing!
        };
        
        EOS_Achievements_PlayerAchievement* OutAchievement;
        auto result = EOS_Achievements_CopyPlayerAchievementByIndex(
            getHAchievements(),
            &CopyAchievementOptions,
            &OutAchievement
        );
        
        if(result != EOS_EResult::EOS_Success){
            Logger::error("Failed to copy player achievement by index %d. "
                "Result string: %s", i, EOS_EResult_ToString(result));
            continue;
        }

        printPlayerAchievement(OutAchievement);

        // Update our achievement array if this achievement is unlocked
        if(OutAchievement->UnlockTime != -1){
            findAchievement(OutAchievement->AchievementId, [](Overlay_Achievement& achievement){
                achievement.UnlockState = UnlockState::Unlocked;
            });
        }

        EOS_Achievements_PlayerAchievement_Release(OutAchievement);
    }

    // Initialize Overlay
    Overlay::Init(ScreamAPI::thisDLL, &achievements, unlockAchievement);
}


// ============================================================================
// PATCH 3: achievement_manager.cpp - Line ~301-315
// Function: queryAchievementDefinitions() - Beginning
// ============================================================================

// ENHANCED VERSION WITH BETTER DIAGNOSTICS:
void queryAchievementDefinitions(){
    auto productUserId = getProductUserId();
    auto epicAccountId = getEpicAccountId();
    auto hAchievements = getHAchievements();
    
    Logger::debug("[ACH] queryAchievementDefinitions called");
    Logger::debug("[ACH]   ProductUserId: %p", productUserId);
    Logger::debug("[ACH]   EpicAccountId: %p", epicAccountId);
    Logger::debug("[ACH]   HAchievements: %p", hAchievements);
    
    if(hAchievements == nullptr) {
        Logger::error("[ACH] Cannot query achievements - HAchievements is NULL");
        
        // NEW: Retry logic for delayed platform initialization
        static int nullRetryCount = 0;
        if(nullRetryCount < 5) {
            nullRetryCount++;
            Logger::warn("[ACH] Platform not ready yet, scheduling retry %d/5 in 3 seconds", nullRetryCount);
            
            std::thread([]() {
                Sleep(3000);
                queryAchievementDefinitions();
            }).detach();
        } else {
            Logger::error("[ACH] Platform still NULL after 5 retries (15 seconds). Giving up.");
            Logger::error("[ACH] Possible causes:");
            Logger::error("[ACH]   1. Game's EOS SDK version incompatible with ScreamAPI");
            Logger::error("[ACH]   2. Game hasn't initialized EOS platform yet");
            Logger::error("[ACH]   3. EOS_Platform_Create failed in the game");
        }
        return;
    }
    
    if(productUserId == nullptr && epicAccountId == nullptr) {
        Logger::warn("[ACH] Both ProductUserId and EpicAccountId are NULL - user may not be logged in");
        Logger::warn("[ACH] Will attempt query anyway - may fail with InvalidParameters");
    }
    
    // Query achievement definitions
    EOS_Achievements_QueryDefinitionsOptions QueryDefinitionsOptions = {
            1, // Use legacy API, since older DLLs will throw EOS_IncompatibleVersion error
            productUserId,
            epicAccountId,
            nullptr,
            0
    };

    Logger::debug("[ACH] Calling EOS_Achievements_QueryDefinitions");
    EOS_Achievements_QueryDefinitions(
        hAchievements,
        &QueryDefinitionsOptions,
        nullptr,
        queryDefinitionsComplete
    );
}


// ============================================================================
// PATCH 4: util.cpp / util.h - NEW HELPER FUNCTION
// Add this to your utility functions
// ============================================================================

// In util.h - ADD THIS DECLARATION:
/*
bool isEOSPlatformReady();
void logPlatformStatus();
*/

// In util.cpp - ADD THESE IMPLEMENTATIONS:

/**
 * Checks if the EOS platform and all required interfaces are available
 * @return true if platform is fully initialized and ready
 */
bool isEOSPlatformReady() {
    auto platform = ScreamAPI::Platform();
    if(platform == nullptr) {
        Logger::error("[UTIL] EOS Platform is NULL - not initialized");
        return false;
    }
    
    auto hAchievements = EOS_Platform_GetAchievementsInterface(platform);
    if(hAchievements == nullptr) {
        Logger::error("[UTIL] Achievements interface is NULL even though Platform exists");
        Logger::error("[UTIL] This suggests the game's EOS SDK version is incompatible");
        return false;
    }
    
    Logger::debug("[UTIL] EOS Platform is ready");
    return true;
}

/**
 * Logs detailed status of EOS platform and all interfaces
 * Useful for debugging initialization issues
 */
void logPlatformStatus() {
    auto platform = ScreamAPI::Platform();
    
    Logger::debug("[UTIL] ========== EOS Platform Status ==========");
    Logger::debug("[UTIL] Platform Handle:     %p", platform);
    
    if(platform == nullptr) {
        Logger::error("[UTIL] Platform is NULL - cannot check interfaces");
        Logger::debug("[UTIL] ==========================================");
        return;
    }
    
    auto hConnect = EOS_Platform_GetConnectInterface(platform);
    auto hAuth = EOS_Platform_GetAuthInterface(platform);
    auto hAchievements = EOS_Platform_GetAchievementsInterface(platform);
    auto hEcom = EOS_Platform_GetEcomInterface(platform);
    
    Logger::debug("[UTIL] Connect Interface:   %p %s", hConnect, hConnect ? "OK" : "NULL");
    Logger::debug("[UTIL] Auth Interface:      %p %s", hAuth, hAuth ? "OK" : "NULL");
    Logger::debug("[UTIL] Achievements Int:    %p %s", hAchievements, hAchievements ? "OK" : "NULL");
    Logger::debug("[UTIL] Ecom Interface:      %p %s", hEcom, hEcom ? "OK" : "NULL");
    
    auto productUserId = getProductUserId();
    auto epicAccountId = getEpicAccountId();
    
    Logger::debug("[UTIL] Product User ID:     %p %s", productUserId, productUserId ? "OK" : "NULL");
    Logger::debug("[UTIL] Epic Account ID:     %p %s", epicAccountId, epicAccountId ? "OK" : "NULL");
    Logger::debug("[UTIL] ==========================================");
}


// ============================================================================
// PATCH 5: achievement_manager.cpp - Enhanced init() function
// Replace the entire init() function with this version
// ============================================================================

void init() {
    static bool init = false;
    static int initAttempts = 0;

    if(!init && Config::EnableOverlay()) {
        initAttempts++;
        
        Logger::ovrly("Achievement Manager: Initializing... (Attempt %d)", initAttempts);
        Logger::debug("[ACH] init(): Starting achievement manager initialization");
        
        // NEW: Check platform status before proceeding
        logPlatformStatus();
        
        if(!isEOSPlatformReady()) {
            Logger::error("[ACH] EOS Platform not ready - cannot initialize achievement manager");
            
            if(initAttempts < 5) {
                Logger::warn("[ACH] Scheduling retry in 3 seconds (attempt %d/5)", initAttempts + 1);
                std::thread([]() {
                    Sleep(3000);
                    init();  // Retry
                }).detach();
            } else {
                Logger::error("[ACH] Failed to initialize after 5 attempts. Achievement manager disabled.");
                Logger::error("[ACH] Check the following:");
                Logger::error("[ACH]   1. Does game use EOS SDK v1.16+? (ScreamAPI uses v1.13)");
                Logger::error("[ACH]   2. Did game successfully call EOS_Platform_Create?");
                Logger::error("[ACH]   3. Is ScreamAPI's DLL proxy working correctly?");
            }
            return;
        }
        
        init = true;
        Logger::info("[ACH] Platform is ready - proceeding with achievement initialization");
        Logger::debug("[ACH] init(): About to call queryAchievementDefinitions");
        queryAchievementDefinitions();
        Logger::debug("[ACH] init(): About to subscribe to notifications");
        subscribeToAchievementNotifications();
        Logger::debug("[ACH] init(): Achievement manager initialization complete");
    }
}


// ============================================================================
// PATCH 6: dllmain.cpp or init code - Better initialization timing
// Replace your current deferred initialization with this
// ============================================================================

// ORIGINAL (in DllMain or similar):
/*
if(Config::EnableOverlay()) {
    Logger::debug("Scheduling deferred overlay initialization");
    std::thread([hModule]() {
        Sleep(10000);
        Logger::debug("Executing deferred overlay initialization");
        AchievementManager::initWithOverlay(hModule);
    }).detach();
}
*/

// ENHANCED VERSION WITH PLATFORM POLLING:
if(Config::EnableOverlay()) {
    Logger::debug("Scheduling deferred overlay initialization with platform polling");
    
    std::thread([hModule]() {
        const int MAX_WAIT_SECONDS = 30;
        const int POLL_INTERVAL_MS = 1000;
        int elapsedSeconds = 0;
        
        Logger::debug("Waiting for game to initialize EOS platform...");
        
        // Poll for platform readiness
        while(elapsedSeconds < MAX_WAIT_SECONDS) {
            Sleep(POLL_INTERVAL_MS);
            elapsedSeconds++;
            
            // Check if platform is ready
            auto platform = ScreamAPI::Platform();
            if(platform != nullptr) {
                auto hAchievements = EOS_Platform_GetAchievementsInterface(platform);
                if(hAchievements != nullptr) {
                    Logger::info("EOS Platform detected as ready after %d seconds", elapsedSeconds);
                    Logger::debug("Executing overlay initialization");
                    AchievementManager::initWithOverlay(hModule);
                    return;  // Success!
                }
            }
            
            // Log progress every 5 seconds
            if(elapsedSeconds % 5 == 0) {
                Logger::debug("Still waiting for EOS platform... (%d/%d seconds)", 
                             elapsedSeconds, MAX_WAIT_SECONDS);
            }
        }
        
        // Timed out
        Logger::error("Timed out waiting for EOS platform after %d seconds", MAX_WAIT_SECONDS);
        Logger::error("Achievement overlay will not be available");
        Logger::error("Possible causes:");
        Logger::error("  1. Game doesn't use EOS Achievements (check game documentation)");
        Logger::error("  2. Game's EOS SDK version incompatible with ScreamAPI v1.13");
        Logger::error("  3. Game uses delayed authentication flow");
        
    }).detach();
}


// ============================================================================
// BONUS PATCH: eos_init.cpp - Enhanced Platform Creation
// If you have manual platform creation, replace with this
// ============================================================================

EOS_HPlatform createEOSPlatform(const char* productId, const char* sandboxId, const char* deploymentId) {
    Logger::info("Attempting to create EOS Platform...");
    
    EOS_Platform_Options PlatformOptions = {};
    PlatformOptions.ProductId = productId;
    PlatformOptions.SandboxId = sandboxId;
    PlatformOptions.DeploymentId = deploymentId;
    PlatformOptions.bIsServer = EOS_FALSE;
    PlatformOptions.Reserved = nullptr;
    
    // Try different API versions to find compatibility
    const int MIN_API_VERSION = 8;
    const int MAX_API_VERSION = 13;
    
    for(int apiVer = MAX_API_VERSION; apiVer >= MIN_API_VERSION; apiVer--) {
        PlatformOptions.ApiVersion = apiVer;
        
        Logger::debug("Trying EOS_Platform_Create with ApiVersion %d", apiVer);
        Logger::debug("  ProductId: %s", productId ? productId : "NULL");
        Logger::debug("  SandboxId: %s", sandboxId ? sandboxId : "NULL");
        Logger::debug("  DeploymentId: %s", deploymentId ? deploymentId : "NULL");
        
        EOS_HPlatform platform = EOS_Platform_Create(&PlatformOptions);
        
        if(platform != nullptr) {
            Logger::info("SUCCESS: EOS Platform created with ApiVersion %d", apiVer);
            Logger::info("  Platform Handle: %p", platform);
            
            // Verify interfaces are accessible
            auto hAchievements = EOS_Platform_GetAchievementsInterface(platform);
            auto hConnect = EOS_Platform_GetConnectInterface(platform);
            
            Logger::debug("  Achievements Interface: %p", hAchievements);
            Logger::debug("  Connect Interface: %p", hConnect);
            
            return platform;
        } else {
            Logger::debug("  ApiVersion %d: FAILED (returned NULL)", apiVer);
        }
    }
    
    Logger::error("FAILED: Could not create EOS Platform with any ApiVersion (%d-%d)", 
                  MIN_API_VERSION, MAX_API_VERSION);
    Logger::error("This suggests:");
    Logger::error("  1. Invalid credentials (ProductId/SandboxId/DeploymentId)");
    Logger::error("  2. Game's EOS SDK requires ApiVersion > %d", MAX_API_VERSION);
    Logger::error("  3. EOS SDK DLL is corrupted or missing");
    
    return nullptr;
}


// ============================================================================
// END OF PATCHES
// ============================================================================

/*
TESTING CHECKLIST:
- [ ] Compiled without errors
- [ ] Tested in Beholder (32-bit)
- [ ] Check log for: "[ACH] Platform is ready"
- [ ] Check log for: "EOS Platform detected as ready after X seconds"
- [ ] Check log for: "Achievement definitions query succeeded"
- [ ] Check log for: "Found N achievement definitions"
- [ ] Verify overlay shows achievements
- [ ] Test unlocking an achievement via overlay

EXPECTED SUCCESS LOG:
[INFO]  Successfully loaded original EOS SDK
[DEBUG] Scheduling deferred overlay initialization with platform polling
[DEBUG] Waiting for game to initialize EOS platform...
[INFO]  EOS Platform detected as ready after 8 seconds
[DEBUG] Executing overlay initialization
[OVRLY] Achievement Manager: Initializing... (Attempt 1)
[UTIL]  ========== EOS Platform Status ==========
[UTIL]  Platform Handle:     0x12345678
[UTIL]  Achievements Int:    0xABCDEF00 OK
[UTIL]  Product User ID:     0x11223344 OK
[UTIL]  ==========================================
[INFO]  [ACH] Platform is ready - proceeding with achievement initialization
[DEBUG] [ACH] Calling EOS_Achievements_QueryDefinitions
[DEBUG] queryDefinitionsComplete called with ResultCode: 0
[INFO]  Found 25 achievement definitions
[INFO]  Found 3 unlocked achievements
[OVRLY] Achievement Manager: Ready
*/
