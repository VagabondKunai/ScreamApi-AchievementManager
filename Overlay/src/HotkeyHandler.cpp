#include "pch.h"
#include "HotkeyHandler.h"
#include "achievement_manager.h"
#include "Overlay.h"
#include <fstream>
#include <string>
#include <algorithm>
#include <cctype>
#include <atomic>
#include <thread>

namespace HotkeyHandler {

    static HWND hwndHotkey = nullptr;
    static const UINT HOTKEY_ID_UNLOCK_ALL = 1;
    static const UINT HOTKEY_ID_UNLOCK_LIST = 2;
    static std::atomic<bool> keepRunning{false};
    static std::thread messageThread;

    // Helper: trim whitespace
    static std::string trim(const std::string& s) {
        size_t start = s.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        size_t end = s.find_last_not_of(" \t\r\n");
        return s.substr(start, end - start + 1);
    }

    // Unlock all achievements
    static void UnlockAllAchievements() {
        if (Overlay::achievements) {
            int count = 0;
            for (auto& ach : *Overlay::achievements) {
                if (ach.UnlockState == UnlockState::Locked) {
                    Overlay::unlockAchievement(&ach);
                    count++;
                    Logger::info("[HOTKEY] Unlocking all: %s", ach.AchievementId);
                }
            }
            Logger::info("[HOTKEY] Unlocked %d achievements.", count);
        } else {
            Logger::error("[HOTKEY] Achievements list not available.");
        }
    }

    // Unlock from file (unlock_list.txt in game folder)
    static void UnlockFromFile() {
        char path[MAX_PATH];
        GetModuleFileNameA(NULL, path, MAX_PATH);
        std::string exePath(path);
        std::string dir = exePath.substr(0, exePath.find_last_of("\\/"));
        std::string listFile = dir + "\\unlock_list.txt";

        std::ifstream file(listFile);
        if (!file.is_open()) {
            Logger::error("[HOTKEY] Could not open %s", listFile.c_str());
            return;
        }

        std::string line;
        int count = 0;
        while (std::getline(file, line)) {
            std::string id = trim(line);
            if (id.empty()) continue;
            AchievementManager::findAchievement(id.c_str(), [&](Overlay_Achievement& ach) {
                if (ach.UnlockState == UnlockState::Locked) {
                    Overlay::unlockAchievement(&ach);
                    count++;
                    Logger::info("[HOTKEY] Unlocking specific: %s", ach.AchievementId);
                } else {
                    Logger::info("[HOTKEY] Already unlocked: %s", ach.AchievementId);
                }
            });
        }
        file.close();
        Logger::info("[HOTKEY] Unlocked %d achievements from %s", count, listFile.c_str());
    }

    // Window procedure for the hidden message window
    LRESULT CALLBACK HotkeyWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (msg == WM_HOTKEY) {
            if (wParam == HOTKEY_ID_UNLOCK_ALL) {
                UnlockAllAchievements();
                return 0;
            } else if (wParam == HOTKEY_ID_UNLOCK_LIST) {
                UnlockFromFile();
                return 0;
            }
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    // Message loop – runs in its own thread
    static void MessageLoop() {
        MSG msg;
        while (keepRunning) {
            // Use GetMessage (blocks) instead of PeekMessage to reduce CPU usage
            if (GetMessage(&msg, NULL, 0, 0)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        Logger::info("Hotkey message loop exiting.");
    }

    void Start() {
        if (keepRunning) {
            Logger::info("Hotkey handler already running.");
            return;
        }

        // Register window class
        WNDCLASSEX wc = {0};
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.lpfnWndProc = HotkeyWndProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = L"ScreamAPI_HotkeyWindow";
        if (!RegisterClassEx(&wc)) {
            Logger::error("Failed to register hotkey window class (error %d)", GetLastError());
            return;
        }

        // Create hidden message-only window
        hwndHotkey = CreateWindowEx(0, wc.lpszClassName, L"HotkeyWindow", 0, 0, 0, 0, 0,
                                    HWND_MESSAGE, NULL, wc.hInstance, NULL);
        if (!hwndHotkey) {
            Logger::error("Failed to create hotkey window (error %d)", GetLastError());
            return;
        }

        // Register hotkeys
        if (!RegisterHotKey(hwndHotkey, HOTKEY_ID_UNLOCK_ALL, MOD_CONTROL | MOD_SHIFT, 'U')) {
            Logger::error("Failed to register hotkey Ctrl+Shift+U (error %d)", GetLastError());
        } else {
            Logger::info("Global hotkey Ctrl+Shift+U registered successfully.");
        }

        if (!RegisterHotKey(hwndHotkey, HOTKEY_ID_UNLOCK_LIST, MOD_CONTROL | MOD_SHIFT, 'L')) {
            Logger::error("Failed to register hotkey Ctrl+Shift+L (error %d)", GetLastError());
        } else {
            Logger::info("Global hotkey Ctrl+Shift+L registered successfully.");
        }

        // Start message loop thread
        keepRunning = true;
        messageThread = std::thread(MessageLoop);
        messageThread.detach();
        Logger::info("Hotkey message loop thread started.");
    }

    void Stop() {
        keepRunning = false;
        if (hwndHotkey) {
            // Post a quit message to wake up GetMessage
            PostMessage(hwndHotkey, WM_QUIT, 0, 0);
            UnregisterHotKey(hwndHotkey, HOTKEY_ID_UNLOCK_ALL);
            UnregisterHotKey(hwndHotkey, HOTKEY_ID_UNLOCK_LIST);
            DestroyWindow(hwndHotkey);
            hwndHotkey = nullptr;
        }
        // The message thread will exit when keepRunning is false and GetMessage returns false
        Logger::info("Hotkey handler stopped.");
    }
}