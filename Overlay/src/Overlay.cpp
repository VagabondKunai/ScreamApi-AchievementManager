#include "pch.h"
#include "Overlay.h"
#include "Loader.h"
#include "achievement_manager_ui.h"
#include <thread>
#include <future>

#define POPUP_DURATION_MS	4500

// Forward declaration, as suggested by imgui_impl_win32.cpp#L270
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Overlay {

HRESULT(WINAPI* originalPresent) (IDXGISwapChain*, UINT, UINT);
HRESULT(WINAPI* originalResizeBuffers) (IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
LRESULT(WINAPI* originalWindowProc)(HWND, UINT, WPARAM, LPARAM);

HWND gWindow = nullptr;
ID3D11Device* gD3D11Device = nullptr;
ID3D11DeviceContext* gContext = nullptr;
ID3D11RenderTargetView* gRenderTargetView = nullptr;

bool bKieroInit = false;
bool bInit = false;
bool bShowInitPopup = true;
bool bShowAchievementManager = false;

// Mouse wheel delta accumulator
static float g_MouseWheelDelta = 0.0f;

Achievements* achievements = nullptr;
UnlockAchievementFunction* unlockAchievement = nullptr;

LRESULT WINAPI WindowProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	// Handle keyboard shortcuts even when overlay is closed
	if (uMsg == WM_KEYDOWN) {
		// Shift + F5 pressed?
		if (GetKeyState(VK_SHIFT) & 0x8000 && wParam == VK_F5) {
			bShowInitPopup = false; // Hide the popup
			bShowAchievementManager = !bShowAchievementManager; // Toggle the overlay
			return 0;
		}
	}

	// When overlay is open, let ImGui process the message (only for standard messages)
	if (bShowAchievementManager) {
		// Optional: translate pointer messages for some games (Civilization VI fix)
		UINT translatedMsg = uMsg;
		switch (uMsg) {
		case WM_POINTERDOWN: translatedMsg = WM_LBUTTONDOWN; break;
		case WM_POINTERUP:   translatedMsg = WM_LBUTTONUP;   break;
		case WM_POINTERWHEEL:translatedMsg = WM_MOUSEWHEEL;  break;
		case WM_POINTERUPDATE:translatedMsg = WM_SETCURSOR;  break;
		default: break;
		}
		ImGui_ImplWin32_WndProcHandler(hWnd, translatedMsg, wParam, lParam);
		// Do NOT return true here – we still want the game to process non-mouse messages
		// Return only if we want to block the message entirely (e.g., for shortcuts)
	}

	return CallWindowProc(originalWindowProc, hWnd, uMsg, wParam, lParam);
}

// Manual mouse polling for games that don't send messages through WindowProc
void UpdateImGuiMouseInput() {
	ImGuiIO& io = ImGui::GetIO();

	// Get mouse position
	POINT mousePos;
	GetCursorPos(&mousePos);
	ScreenToClient(gWindow, &mousePos);
	io.MousePos.x = (float)mousePos.x;
	io.MousePos.y = (float)mousePos.y;

	// Get mouse button states
	io.MouseDown[0] = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
	io.MouseDown[1] = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
	io.MouseDown[2] = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;

	// Mouse wheel delta (accumulated from hook or polling)
	io.MouseWheel = g_MouseWheelDelta;
	g_MouseWheelDelta = 0.0f;

	// Optional: also read wheel directly via GetAsyncKeyState (less accurate)
	// short zDelta = GetAsyncKeyState(VK_MBUTTON) >> 8; // not reliable
}

// Hook for WM_MOUSEWHEEL (can be installed via SetWindowsHookEx if needed)
// For simplicity, we'll accumulate wheel delta from the window procedure
// But since some games don't send WM_MOUSEWHEEL, we also poll directly in UpdateImGuiMouseInput.
// The following function can be called from WindowProc when a wheel message arrives.
void AccumulateMouseWheel(float delta) {
	g_MouseWheelDelta += delta;
}

HRESULT WINAPI hookedPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
	if (!bInit) {
		Logger::ovrly("hookedPresent: Initializing overlay");
		if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&gD3D11Device))) {
			gD3D11Device->GetImmediateContext(&gContext);
			DXGI_SWAP_CHAIN_DESC sd;
			pSwapChain->GetDesc(&sd);
			gWindow = sd.OutputWindow;
			Logger::ovrly("hookedPresent: Got D3D11 device, window handle: %p", gWindow);
			ID3D11Texture2D* pBackBuffer;
			if (SUCCEEDED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer))) {
				Logger::ovrly("hookedPresent: Got back buffer");
				if (SUCCEEDED(gD3D11Device->CreateRenderTargetView(pBackBuffer, NULL, &gRenderTargetView))) {
					Logger::ovrly("hookedPresent: Created render target view");
				}
				else {
					Logger::error("hookedPresent: Failed to create render target view");
				}
				pBackBuffer->Release();
			}
			else {
				Logger::error("hookedPresent: Failed to get back buffer");
				return originalPresent(pSwapChain, SyncInterval, Flags);
			}
			originalWindowProc = (WNDPROC)SetWindowLongPtr(gWindow, GWLP_WNDPROC, (LONG_PTR)WindowProc);
			if (originalWindowProc == NULL) {
				Logger::error("Failed to SetWindowLongPtr. Error code: %d", GetLastError());
				return originalPresent(pSwapChain, SyncInterval, Flags);
			}
			Logger::ovrly("hookedPresent: Initializing ImGui");
			AchievementManagerUI::InitImGui(gWindow, gD3D11Device, gContext);
			bInit = true;
			Logger::ovrly("hookedPresent: Overlay initialization complete");
			Loader::AsyncLoadIcons();
		}
		else {
			Logger::error("hookedPresent: Failed to get D3D11 device");
			return originalPresent(pSwapChain, SyncInterval, Flags);
		}
	}

	// Begin ImGui frame
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// Manually update mouse input for games that don't use standard window messages
	UpdateImGuiMouseInput();

	// Show/hide ImGui cursor based on overlay state
	ImGui::GetIO().MouseDrawCursor = bShowAchievementManager;

	// Draw UI
	if (bShowInitPopup)
		AchievementManagerUI::DrawInitPopup();

	if (bShowAchievementManager)
		AchievementManagerUI::DrawAchievementList();

	ImGui::Render();

	// Set render target and draw
	if (gRenderTargetView)
		gContext->OMSetRenderTargets(1, &gRenderTargetView, NULL);
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	return originalPresent(pSwapChain, SyncInterval, Flags);
}

HRESULT WINAPI hookedResizeBuffer(IDXGISwapChain* pThis, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags) {
	Logger::debug("hookedResizeBuffer");

	if (bInit) {
		AchievementManagerUI::ShutdownImGui();

		// Restore original WndProc
		SetWindowLongPtr(gWindow, GWLP_WNDPROC, (LONG_PTR)originalWindowProc);

		// Release RTV
		if (gRenderTargetView) {
			gRenderTargetView->Release();
			gRenderTargetView = nullptr;
		}

		bInit = false;
	}
	return originalResizeBuffers(pThis, BufferCount, Width, Height, NewFormat, SwapChainFlags);
}

// Table with method indices: https://github.com/Rebzzel/kiero/blob/master/METHODSTABLE.txt
#define D3D11_Present		8
#define D3D11_ResizeBuffers	13

static std::mutex initMutex;
void Init(HMODULE hMod, Achievements* pAchievements, UnlockAchievementFunction* fnUnlockAchievement) {
	achievements = pAchievements;
	unlockAchievement = fnUnlockAchievement;

	// Init asynchronously to keep the main thread going
	Logger::ovrly("Overlay::Init: Starting async initialization");
	static auto initJob = std::async(std::launch::async, []() {
		Logger::ovrly("Overlay::Init: Async init thread started");
		{
			std::lock_guard<std::mutex> guard(initMutex);

			if (bKieroInit) {
				Logger::ovrly("Overlay::Init: Already initialized");
				return;
			}

			Logger::ovrly("Overlay::Init: Calling kiero::init");
			auto result = kiero::init(kiero::RenderType::D3D11);
			if (result != kiero::Status::Success) {
				Logger::debug("Kiero: result code = %d", result);
				if (result == kiero::Status::ModuleNotFoundError)
					Logger::error("Failed to initialize kiero. Are you sure you are running a DirectX 11 game?");
				else
					Logger::error("Failed to initialize kiero. Error code: %d", result);
				return;
			}
			bKieroInit = true;
			Logger::ovrly("Kiero: Successfully initialized");

			// Hook Present
			Logger::ovrly("Overlay::Init: Hooking Present");
			kiero::bind(D3D11_Present, (void**)&originalPresent, hookedPresent);
			Logger::ovrly("Kiero: Successfully hooked Present");

			// Hook ResizeBuffers
			Logger::ovrly("Overlay::Init: Hooking ResizeBuffers");
			kiero::bind(D3D11_ResizeBuffers, (void**)&originalResizeBuffers, hookedResizeBuffer);
			Logger::ovrly("Kiero: Successfully hooked ResizeBuffers");

			// Hide the popup after POPUP_DURATION_MS time
			static auto hidePopupJob = std::async(std::launch::async, [&]() {
				Sleep(POPUP_DURATION_MS);
				bShowInitPopup = false;
			});
		}
	});
}

void Shutdown() {
	AchievementManagerUI::ShutdownImGui();
	SetWindowLongPtr(gWindow, GWLP_WNDPROC, (LONG_PTR)originalWindowProc);
	kiero::shutdown();
	Logger::ovrly("Kiero: Shutdown");
}

} // namespace Overlay