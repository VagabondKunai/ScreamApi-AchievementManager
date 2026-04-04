#pragma once
#include "pch.h"
#include "Overlay_types.h"

namespace Overlay {

extern ID3D11Device* gD3D11Device;
extern ID3D11DeviceContext* gContext;
extern Achievements* achievements;
extern UnlockAchievementFunction* unlockAchievement;

// Global flag for the startup popup (used by AchievementManagerUI)
extern bool bShowInitPopup;

void Init(HMODULE hMod, Achievements* pAchievements, UnlockAchievementFunction* pUnlockAchievement);
void Shutdown();

} // namespace Overlay