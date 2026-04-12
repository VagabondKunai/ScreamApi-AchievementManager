#pragma once
#include <Windows.h>
#include <d3d11.h>

namespace AchievementManagerUI {
    void InitImGui(HWND hWnd, ID3D11Device* device, ID3D11DeviceContext* context);
    void ShutdownImGui();
    void DrawInitPopup();
    void DrawAchievementList();
    void RequestFocus();   // <-- NEW (for focus on open)
}