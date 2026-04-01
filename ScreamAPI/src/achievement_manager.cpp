#include "pch.h"
#include "achievement_manager.h"
#include "ScreamAPI.h"
#include "util.h"
#include "eos-sdk/eos_achievements.h"
#include "Overlay.h"

using namespace Util;

namespace AchievementManager{

Achievements achievements;

// Forward declaration
void queryAchievementDefinitions();

void printAchievementDefinition(EOS_Achievements_DefinitionV2* definition){
	if(definition == nullptr){
		Logger::ach("Invalid Achievement Definition");
	} else {
		std::stringstream ss;
		ss
			<< "[Achievement Definition]\n"
			<< "\t\t\t""AchievementId: " << definition->AchievementId << "\n"
			<< "\t\t\t""IsHidden: " << definition->bIsHidden << "\n"
			<< "\t\t\t""FlavorText: " << definition->FlavorText << "\n"
			<< "\t\t\t""LockedDescription: " << definition->LockedDescription << "\n"
			<< "\t\t\t""LockedDisplayName: " << definition->LockedDisplayName << "\n"
			<< "\t\t\t""LockedIconURL: " << definition->LockedIconURL << "\n"
			<< "\t\t\t""UnlockedDescription: " << definition->UnlockedDescription << "\n"
			<< "\t\t\t""UnlockedDisplayName: " << definition->UnlockedDisplayName << "\n"
			<< "\t\t\t""UnlockedIconURL: " << definition->UnlockedIconURL << "\n"
			<< "\t\t\t""StatThresholdsCount: " << definition->StatThresholdsCount << "\n";

		for(unsigned int i = 0; i < definition->StatThresholdsCount; i++){
			ss
				<< "\t\t\t\t""[StatThreshold] "
				<< "Name: " << definition->StatThresholds[i].Name << "; "
				<< "Threshold: " << definition->StatThresholds[i].Threshold;
		}
		Logger::ach("%s", ss.str().c_str());
	}
}

void printPlayerAchievement(EOS_Achievements_PlayerAchievement* achievement){
	if(achievement == nullptr){
		Logger::ach("Invalid Player Achievement");
	} else{
		std::stringstream ss;
		ss
			<< "[Player Achievement]\n"
			<< "\t\t\t""AchievementId: " << achievement->AchievementId << "\n"
			<< "\t\t\t""Description: " << achievement->Description << "\n"
			<< "\t\t\t""DisplayName: " << achievement->DisplayName << "\n"
			<< "\t\t\t""FlavorText: " << achievement->FlavorText << "\n"
			<< "\t\t\t""IconURL: " << achievement->IconURL << "\n"
			<< "\t\t\t""Progress: " << achievement->Progress << "\n"
			<< "\t\t\t""StatInfoCount: " << achievement->StatInfoCount << "\n";

		for(int i = 0; i < achievement->StatInfoCount; i++){
			ss
				<< "\t\t\t\t""[StatInfo] "
				<< "Name: " << achievement->StatInfo[i].Name << "; "
				<< "CurrentValue: " << achievement->StatInfo[i].CurrentValue << "; "
				<< "ThresholdValue: " << achievement->StatInfo[i].ThresholdValue;
		}
		Logger::ach("%s", ss.str().c_str());
	}
}

void findAchievement(const char* achievementID, std::function<void(Overlay_Achievement&)> callback){
	for(auto& achievement : achievements){
		if(!strcmp(achievement.AchievementId, achievementID)){
			callback(achievement);
			return;
		}
	}
	Logger::error("Could not find achievement with id: %s", achievementID);
}

void unlockAchievement(Overlay_Achievement* achievement){
	achievement->UnlockState = UnlockState::Unlocking;

	EOS_Achievements_UnlockAchievementsOptions Options = {
		EOS_ACHIEVEMENTS_UNLOCKACHIEVEMENTS_API_LATEST,
		getProductUserId(),
		&achievement->AchievementId,
		1
	};

	EOS_Achievements_UnlockAchievements(getHAchievements(), &Options, achievement,
		[](const EOS_Achievements_OnUnlockAchievementsCompleteCallbackInfo* Data){
		auto achievement = (Overlay_Achievement*)Data->ClientData;

		if(Data->ResultCode == EOS_EResult::EOS_Success){
			Logger::info("Successfully unlocked the achievement: %s", achievement->AchievementId);
		} else {
			achievement->UnlockState = UnlockState::Locked;
			Logger::error("Failed to unlock the achievement: %s. Error string: %s",
				achievement->AchievementId,
				EOS_EResult_ToString(Data->ResultCode));
		}
	});
}

void EOS_CALL queryPlayerAchievementsComplete(const EOS_Achievements_OnQueryPlayerAchievementsCompleteCallbackInfo* Data){
	Logger::debug("queryPlayerAchievementsComplete");

	if(Data->ResultCode != EOS_EResult::EOS_Success){
		Logger::error("Failed to query player achievements. Result string: %s",
			EOS_EResult_ToString(Data->ResultCode));
		Overlay::Init(ScreamAPI::thisDLL, &achievements, unlockAchievement);
		return;
	}

	static EOS_Achievements_GetPlayerAchievementCountOptions GetCountOptions{
		EOS_ACHIEVEMENTS_GETPLAYERACHIEVEMENTCOUNT_API_LATEST,
		getProductUserId()
	};
	auto playerAchievementsCount = EOS_Achievements_GetPlayerAchievementCount(getHAchievements(), &GetCountOptions);
	for(unsigned int i = 0; i < playerAchievementsCount; i++){
		EOS_Achievements_CopyPlayerAchievementByIndexOptions CopyAchievementOptions{
			EOS_ACHIEVEMENTS_COPYPLAYERACHIEVEMENTBYINDEX_API_LATEST,
			getProductUserId(),      // TargetUserId
			i,                       // AchievementIndex
			getProductUserId()       // LocalUserId
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

		if(OutAchievement->UnlockTime != -1){
			findAchievement(OutAchievement->AchievementId, [](Overlay_Achievement& achievement){
				achievement.UnlockState = UnlockState::Unlocked;
			});
		}

		EOS_Achievements_PlayerAchievement_Release(OutAchievement);
	}

	Overlay::Init(ScreamAPI::thisDLL, &achievements, unlockAchievement);
}

void EOS_CALL queryDefinitionsComplete(const EOS_Achievements_OnQueryDefinitionsCompleteCallbackInfo* Data){
	Logger::debug("[ACH] queryDefinitionsComplete called with ResultCode: %d", Data->ResultCode);

	if(Data->ResultCode != EOS_EResult::EOS_Success){
		Logger::error("[ACH] Failed to query achievement definitions. Result: %s",
			EOS_EResult_ToString(Data->ResultCode));
		return;
	}

	Logger::debug("[ACH] Achievement definitions query succeeded");

	static EOS_Achievements_GetAchievementDefinitionCountOptions GetCountOptions{
		EOS_ACHIEVEMENTS_GETACHIEVEMENTDEFINITIONCOUNT_API_LATEST
	};
	auto achievementDefinitionCount = EOS_Achievements_GetAchievementDefinitionCount(getHAchievements(), &GetCountOptions);
	for(uint32_t i = 0; i < achievementDefinitionCount; i++){
		static bool useDeprecated = false;

		if(!useDeprecated){
			try{
				EOS_Achievements_CopyAchievementDefinitionV2ByIndexOptions DefinitionOptions{
					EOS_ACHIEVEMENTS_COPYACHIEVEMENTDEFINITIONV2BYINDEX_API_LATEST,
					i
				};
				EOS_Achievements_DefinitionV2* OutDefinition;
				auto result = EOS_Achievements_CopyAchievementDefinitionV2ByIndex(getHAchievements(), &DefinitionOptions, &OutDefinition);
				if(result != EOS_EResult::EOS_Success){
					Logger::error("Failed to copy achievement definition by index %d. "
						"Result string: %s", i, EOS_EResult_ToString(result));
					continue;
				}

				printAchievementDefinition(OutDefinition);

				achievements.push_back(
					{
						copy_c_string(OutDefinition->AchievementId),
						(bool)OutDefinition->bIsHidden,
						UnlockState::Locked,
						copy_c_string(OutDefinition->UnlockedDescription),
						copy_c_string(OutDefinition->UnlockedDisplayName),
						copy_c_string(OutDefinition->UnlockedIconURL),
						nullptr
					}
				);

				EOS_Achievements_DefinitionV2_Release(OutDefinition);
				continue;
			} catch(ScreamAPI::FunctionNotFoundException){
				useDeprecated = true;
			}
		}

		EOS_Achievements_CopyAchievementDefinitionByIndexOptions DefinitionOptions{
			EOS_ACHIEVEMENTS_COPYDEFINITIONBYINDEX_API_LATEST,
			i
		};
		EOS_Achievements_Definition* OutDefinition;
		auto result = EOS_Achievements_CopyAchievementDefinitionByIndex(getHAchievements(), &DefinitionOptions, &OutDefinition);
		if(result != EOS_EResult::EOS_Success){
			Logger::error("Failed to copy (deprecated) achievement definition by index %d. "
				"Result string: %s", i, EOS_EResult_ToString(result));
			continue;
		}

		achievements.push_back(
			{
				copy_c_string(OutDefinition->AchievementId),
				(bool)OutDefinition->bIsHidden,
				UnlockState::Locked,
				copy_c_string(OutDefinition->CompletionDescription),
				copy_c_string(OutDefinition->DisplayName),
				copy_c_string(OutDefinition->UnlockedIconId),
				nullptr
			}
		);

		EOS_Achievements_Definition_Release(OutDefinition);
	}

	EOS_Achievements_QueryPlayerAchievementsOptions QueryAchievementsOptions = {
		EOS_ACHIEVEMENTS_QUERYPLAYERACHIEVEMENTS_API_LATEST,
		getProductUserId(),      // TargetUserId
		getProductUserId()       // LocalUserId
	};

	EOS_Achievements_QueryPlayerAchievements(
		getHAchievements(),
		&QueryAchievementsOptions,
		nullptr,
		queryPlayerAchievementsComplete
	);
}

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
		return;
	}

	if(productUserId == nullptr && epicAccountId == nullptr) {
		Logger::warn("[ACH] Both ProductUserId and EpicAccountId are NULL - user may not be logged in");
		Logger::warn("[ACH] Will attempt query anyway - may fail with InvalidParameters");
	}

	EOS_Achievements_QueryDefinitionsOptions QueryDefinitionsOptions = {
		1,
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

void subscribeToAchievementNotifications(){
	static bool useDeprecated = false;

	if(!useDeprecated){
		try{
			EOS_Achievements_AddNotifyAchievementsUnlockedV2Options NotifyOptions = {
				EOS_ACHIEVEMENTS_ADDNOTIFYACHIEVEMENTSUNLOCKEDV2_API_LATEST
			};

			EOS_Achievements_AddNotifyAchievementsUnlockedV2(getHAchievements(), &NotifyOptions, nullptr,
				[](const EOS_Achievements_OnAchievementsUnlockedCallbackV2Info* Data){
				findAchievement(Data->AchievementId, [](Overlay_Achievement& achievement){
					achievement.UnlockState = UnlockState::Unlocked;
				});
			});

			return;
		} catch(ScreamAPI::FunctionNotFoundException){
			useDeprecated = true;
		}
	}

	EOS_Achievements_AddNotifyAchievementsUnlockedOptions NotifyOptions = {
		EOS_ACHIEVEMENTS_ADDNOTIFYACHIEVEMENTSUNLOCKED_API_LATEST
	};

	EOS_Achievements_AddNotifyAchievementsUnlocked(getHAchievements(), &NotifyOptions, nullptr,
		[](const EOS_Achievements_OnAchievementsUnlockedCallbackInfo* Data){
		for(uint32_t i = 0; i < Data->AchievementsCount; i++){
			findAchievement(Data->AchievementIds[i], [](Overlay_Achievement& achievement){
				achievement.UnlockState = UnlockState::Unlocked;
			});
		}
	});
}

void init() {
	static bool init = false;

	if(!init && Config::EnableOverlay()) {
		init = true;
		Logger::ovrly("Achievement Manager: Waiting 7 seconds for EOS to initialize...");
		Sleep(7000);
		Logger::ovrly("Achievement Manager: Initializing...");
		Logger::debug("[ACH] init(): Starting achievement manager initialization");
		
		queryAchievementDefinitions();
		subscribeToAchievementNotifications();
		Logger::debug("[ACH] init(): Achievement manager initialization complete");
	}
}

void initWithOverlay(void* hModule) {
	Logger::ovrly("Achievement Manager: initWithOverlay called");
	Overlay::Init((HMODULE)hModule, &achievements, unlockAchievement);
	init();
	Logger::ovrly("Achievement Manager: initWithOverlay complete");
}

}