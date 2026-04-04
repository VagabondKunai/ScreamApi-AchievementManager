#pragma once
#include <d3d11.h>
#include <windows.h>

namespace AchievementManagerUI {

// Initialisation and shutdown
void InitImGui(HWND hWnd, ID3D11Device* device, ID3D11DeviceContext* context);
void ShutdownImGui();

// Drawing functions – call these inside your ImGui frame (after NewFrame)
void DrawInitPopup();
void DrawAchievementList();

} // namespace AchievementManagerUI