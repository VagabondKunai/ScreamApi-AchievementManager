#include "pch.h"
#include "achievement_manager_ui.h"
#include "Overlay.h"
#include "achievement_manager.h"   // for refresh()
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>

// Forward declare ImGui Win32 handler (used internally)
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace AchievementManagerUI {

// Fonts (global for the UI)
ImFont* bigFont = nullptr;
ImFont* regularFont = nullptr;
ImFont* achievementNameFont = nullptr;
ImFont* descriptionFont = nullptr;
ImFont* instructionFont = nullptr;

constexpr float regularFontSize = 16.0f;
constexpr float achievementNameFontSize = 18.0f;
constexpr float descriptionFontSize = 17.0f;
constexpr float instructionFontSize = 22.0f;
constexpr float bigFontSize = 28.0f;

static const char* fontPaths[] = {
    "C:\\Windows\\Fonts\\Bahnschrift.ttf",
    "C:\\Windows\\Fonts\\SegoeUI-Variable.ttf",
    "C:\\Windows\\Fonts\\Segoeui.ttf",
    "C:\\Windows\\Fonts\\Arial.ttf"
};

// UI state
static char searchBuffer[128] = "";
static int filterMode = 0; // 0 = All, 1 = Locked only, 2 = Unlocked only
static int currentPage = 0;
static int itemsPerPage = 50;
static const int PAGINATION_THRESHOLD = 200;
static bool initialized = false;

// ----------------------------------------------------------------------------
// Helper: case‑insensitive substring
// ----------------------------------------------------------------------------
static bool containsIgnoreCase(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    auto it = std::search(haystack.begin(), haystack.end(),
        needle.begin(), needle.end(),
        [](char ch1, char ch2) { return std::toupper(ch1) == std::toupper(ch2); });
    return it != haystack.end();
}

// ----------------------------------------------------------------------------
// Helper: copy text to clipboard (for right‑click context menu)
// ----------------------------------------------------------------------------
static void CopyToClipboard(const char* text) {
    if (OpenClipboard(nullptr)) {
        EmptyClipboard();
        size_t len = strlen(text) + 1;
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
        if (hMem) {
            memcpy(GlobalLock(hMem), text, len);
            GlobalUnlock(hMem);
            SetClipboardData(CF_TEXT, hMem);
        }
        CloseClipboard();
        Logger::info("Copied to clipboard: %s", text);
    } else {
        Logger::error("Failed to open clipboard");
    }
}

// ----------------------------------------------------------------------------
// Helper to draw centred text with scaling
// ----------------------------------------------------------------------------
static void FitTextToWindow(ImFont* font, const ImVec4 color, const char* text) {
    ImGui::PushFont(font);
    ImVec2 sz = ImGui::CalcTextSize(text);
    ImGui::PopFont();
    float canvasWidth = ImGui::GetWindowContentRegionWidth();
    float scale = canvasWidth / sz.x;
    if (scale > 1.0f) scale = 1.0f;
    font->Scale = scale;
    ImGui::PushFont(font);
    ImGui::TextColored(color, "%s", text);
    ImGui::PopFont();
    font->Scale = 1.0f;
}

// ----------------------------------------------------------------------------
// ImGui initialisation (D3D11)
// ----------------------------------------------------------------------------
void InitImGui(HWND hWnd, ID3D11Device* device, ID3D11DeviceContext* context) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    // Modern style
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_WindowBg]      = ImVec4(0.08f, 0.08f, 0.10f, 0.96f);
    style.Colors[ImGuiCol_ChildBg]       = ImVec4(0.10f, 0.10f, 0.12f, 0.85f);
    style.Colors[ImGuiCol_Text]          = ImVec4(0.95f, 0.95f, 0.97f, 1.00f);
    style.Colors[ImGuiCol_TextDisabled]  = ImVec4(0.55f, 0.55f, 0.60f, 1.00f);
    style.Colors[ImGuiCol_Button]        = ImVec4(0.20f, 0.45f, 0.80f, 0.80f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.55f, 0.95f, 0.95f);
    style.Colors[ImGuiCol_ButtonActive]  = ImVec4(0.15f, 0.35f, 0.70f, 1.00f);
    style.Colors[ImGuiCol_Header]        = ImVec4(0.20f, 0.45f, 0.80f, 0.50f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.55f, 0.95f, 0.65f);
    style.Colors[ImGuiCol_FrameBg]       = ImVec4(0.16f, 0.16f, 0.18f, 1.00f);
    style.Colors[ImGuiCol_FrameBgHovered]= ImVec4(0.22f, 0.22f, 0.24f, 1.00f);
    style.Colors[ImGuiCol_SliderGrab]    = ImVec4(0.20f, 0.45f, 0.80f, 0.80f);
    style.Colors[ImGuiCol_ScrollbarBg]   = ImVec4(0.10f, 0.10f, 0.12f, 0.60f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.25f, 0.25f, 0.30f, 0.80f);
    style.Colors[ImGuiCol_Separator]     = ImVec4(0.25f, 0.25f, 0.30f, 0.80f);

    style.ChildRounding   = 8.0f;
    style.FrameRounding   = 6.0f;
    style.PopupRounding   = 8.0f;
    style.ScrollbarSize   = 10.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding    = 4.0f;
    style.WindowRounding  = 10.0f;
    style.WindowPadding   = ImVec2(12, 12);
    style.ItemSpacing     = ImVec2(8, 8);
    style.ItemInnerSpacing = ImVec2(6, 6);

    // Load fonts
    io.Fonts->Clear();
    ImFontConfig fontConfig;
    fontConfig.OversampleH = 2;
    fontConfig.OversampleV = 2;
    fontConfig.PixelSnapH = true;
    static const ImWchar ranges[] = { 0x0020, 0xFFFF, 0 };

    bool fontLoaded = false;
    for (const char* path : fontPaths) {
        if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) {
            regularFont = io.Fonts->AddFontFromFileTTF(path, regularFontSize, &fontConfig, ranges);
            if (regularFont) {
                fontConfig.SizePixels = achievementNameFontSize;
                achievementNameFont = io.Fonts->AddFontFromFileTTF(path, achievementNameFontSize, &fontConfig, ranges);
                fontConfig.SizePixels = descriptionFontSize;
                descriptionFont = io.Fonts->AddFontFromFileTTF(path, descriptionFontSize, &fontConfig, ranges);
                fontConfig.SizePixels = instructionFontSize;
                instructionFont = io.Fonts->AddFontFromFileTTF(path, instructionFontSize, &fontConfig, ranges);
                fontConfig.SizePixels = bigFontSize;
                bigFont = io.Fonts->AddFontFromFileTTF(path, bigFontSize, &fontConfig, ranges);
                fontLoaded = true;
                Logger::ovrly("Loaded font: %s", path);
                break;
            }
        }
    }
    if (!fontLoaded) {
        regularFont = io.Fonts->AddFontDefault();
        achievementNameFont = regularFont;
        descriptionFont = regularFont;
        instructionFont = regularFont;
        bigFont = regularFont;
        Logger::ovrly("Using default font");
    }

    // Initialise ImGui backends
    ImGui_ImplWin32_Init(hWnd);
    ImGui_ImplDX11_Init(device, context);
    initialized = true;
    Logger::ovrly("AchievementManagerUI initialised (Win32 + D3D11)");
}

// ----------------------------------------------------------------------------
// Shutdown (called from Overlay)
// ----------------------------------------------------------------------------
void ShutdownImGui() {
    if (initialized) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        initialized = false;
        Logger::ovrly("AchievementManagerUI shut down");
    }
}

// ----------------------------------------------------------------------------
// Draw startup popup (shown for ~4.5 seconds, controlled by Overlay::bShowInitPopup)
// ----------------------------------------------------------------------------
void DrawInitPopup() {
    // Use the global flag from Overlay (declared in Overlay.h)
    if (!Overlay::bShowInitPopup) return;

    static float popupTimer = 0.0f;
    popupTimer += ImGui::GetIO().DeltaTime;
    if (popupTimer >= 4.5f) {
        Overlay::bShowInitPopup = false;
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(560, 140), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.10f, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.85f, 0.0f, 1.0f));
    ImGui::Begin("InitPopup", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
    ImGui::PopStyleColor(2);

    ImGui::PushFont(bigFont);
    ImGui::TextColored(ImVec4(0.85f, 0.85f, 0.85f, 1.0f), "Epic Achievements Unlocker is");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.0f, 0.85f, 0.0f, 1.0f), "online");
    ImGui::PopFont();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushFont(instructionFont);
    ImGui::PushTextWrapPos(540.0f);
    ImGui::TextColored(ImVec4(0.85f, 0.85f, 0.85f, 1.0f), "Press [Shift] + [F5] to open the achievement panel.");
    ImGui::PopTextWrapPos();
    ImGui::PopFont();

    ImGui::End();
}

// ----------------------------------------------------------------------------
// Main achievement list window
// ----------------------------------------------------------------------------
void DrawAchievementList() {
    ImGuiIO& io = ImGui::GetIO();
    
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.10f, 0.96f));
    ImGui::Begin("AchievementManagerUI", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
    ImGui::PopStyleColor();

    ImGui::SetWindowPos(ImVec2(0, 0));
    ImGui::SetWindowSize(ImVec2(450, io.DisplaySize.y));

    FitTextToWindow(bigFont, ImVec4(0.0f, 0.85f, 0.0f, 1.0f), "  EPIC ACHIEVEMENTS UNLOCKER  ");
    ImGui::Separator();
    ImGui::Spacing();

    // Statistics + progress bar
    if (Overlay::achievements && Overlay::achievements->size() > 0) {
        int total = (int)Overlay::achievements->size();
        int unlocked = 0;
        for (const auto& a : *Overlay::achievements) {
            if (a.UnlockState == UnlockState::Unlocked) unlocked++;
        }
        float percent = (total > 0) ? (unlocked * 100.0f / total) : 0.0f;
        
        ImGui::PushFont(regularFont);
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.2f, 1), "%d / %d unlocked (%.1f%%)", unlocked, total, percent);
        ImGui::PopFont();
        ImGui::ProgressBar(percent / 100.0f, ImVec2(-1, 8), "");
        ImGui::Spacing();
    }

    // Filter controls + Refresh button
    ImGui::BeginGroup();
    ImGui::PushItemWidth(150);
    if (ImGui::InputTextWithHint("##search", "Filter...", searchBuffer, sizeof(searchBuffer))) {
        currentPage = 0;   // reset pagination on new search
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button("All")) { filterMode = 0; currentPage = 0; }
    ImGui::SameLine();
    if (ImGui::Button("Locked")) { filterMode = 1; currentPage = 0; }
    ImGui::SameLine();
    if (ImGui::Button("Unlocked")) { filterMode = 2; currentPage = 0; }
    if (strlen(searchBuffer) > 0 || filterMode != 0) {
        ImGui::SameLine();
        if (ImGui::Button("Clear")) {
            searchBuffer[0] = '\0';
            filterMode = 0;
            currentPage = 0;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) {
        AchievementManager::refresh();
    }
    ImGui::EndGroup();
    ImGui::Spacing();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 4.0f);

    ImGui::BeginChild("AchievementList", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

    if (Overlay::achievements == nullptr || Overlay::achievements->size() == 0) {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "Loading achievements...");
    } else {
        // Build filtered list
        std::vector<int> filteredIndices;
        std::string searchLower(searchBuffer);
        for (int i = 0; i < (int)Overlay::achievements->size(); i++) {
            const auto& ach = Overlay::achievements->at(i);
            if (filterMode == 1 && ach.UnlockState != UnlockState::Locked) continue;
            if (filterMode == 2 && ach.UnlockState != UnlockState::Unlocked) continue;
            if (!searchLower.empty()) {
                std::string name(ach.UnlockedDisplayName);
                std::string desc(ach.UnlockedDescription);
                if (!containsIgnoreCase(name, searchLower) && !containsIgnoreCase(desc, searchLower))
                    continue;
            }
            filteredIndices.push_back(i);
        }

        int totalFiltered = (int)filteredIndices.size();
        bool usePagination = (totalFiltered > PAGINATION_THRESHOLD);
        
        if (usePagination) {
            int totalPages = (totalFiltered + itemsPerPage - 1) / itemsPerPage;
            if (currentPage >= totalPages) currentPage = totalPages - 1;
            if (currentPage < 0) currentPage = 0;
            
            ImGui::BeginGroup();
            ImGui::Text("Page %d / %d", currentPage + 1, totalPages);
            ImGui::SameLine();
            if (ImGui::Button("<<")) currentPage = 0;
            ImGui::SameLine();
            if (ImGui::Button("<") && currentPage > 0) currentPage--;
            ImGui::SameLine();
            if (ImGui::Button(">") && currentPage < totalPages - 1) currentPage++;
            ImGui::SameLine();
            if (ImGui::Button(">>")) currentPage = totalPages - 1;
            ImGui::SameLine();
            ImGui::Text("  (%d achievements)", totalFiltered);
            ImGui::EndGroup();
            ImGui::Spacing();
        }

        int startIdx = usePagination ? currentPage * itemsPerPage : 0;
        int endIdx = usePagination ? (std::min)(startIdx + itemsPerPage, totalFiltered) : totalFiltered;

        float rowHeight = 70.0f;
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 startPos = ImGui::GetCursorScreenPos();

        for (int row = startIdx; row < endIdx; row++) {
            int idx = filteredIndices[row];
            auto& achievement = Overlay::achievements->at(idx);
            ImGui::PushID(idx);

            ImVec2 rowMin = ImVec2(startPos.x, startPos.y + (row - startIdx) * rowHeight);
            ImVec2 rowMax = ImVec2(rowMin.x + ImGui::GetWindowWidth(), rowMin.y + rowHeight);
            ImU32 bgColor = (row % 2 == 0) ? IM_COL32(30, 30, 35, 230) : IM_COL32(25, 25, 30, 230);
            drawList->AddRectFilled(rowMin, rowMax, bgColor, 4.0f);

            ImGui::SetCursorScreenPos(rowMin);

            // Star icon
            float iconHeight = 50.0f;
            float iconYOffset = (rowHeight - iconHeight) / 2.0f;
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + iconYOffset);
            ImGui::PushFont(bigFont);
            ImGui::TextUnformatted("★");
            ImGui::PopFont();
            ImGui::SameLine();

            // Text area
            float rightColumnWidth = 85.0f;
            float iconWidth = 50.0f;
            float padding = 15.0f;
            float textWidth = ImGui::GetWindowWidth() - iconWidth - rightColumnWidth - padding;
            if (textWidth < 100.0f) textWidth = 100.0f;

            ImGui::BeginGroup();
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + textWidth);

            ImGui::PushFont(achievementNameFont);
            ImGui::TextColored(ImVec4(1, 1, 1, 1), achievement.UnlockedDisplayName);
            ImGui::PopFont();

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.90f, 1.0f));
            ImGui::PushFont(descriptionFont);
            ImGui::TextWrapped(achievement.UnlockedDescription);
            ImGui::PopFont();
            ImGui::PopStyleColor();

            ImGui::PopTextWrapPos();
            ImGui::EndGroup();

            // Right column (Hidden / Unlock button) – FIXED: changed bIsHidden to IsHidden
            float rightContentHeight = 0.0f;
            if (achievement.IsHidden) rightContentHeight += 20.0f;   // was bIsHidden
            if (achievement.UnlockState == UnlockState::Locked) rightContentHeight += 28.0f;
            else rightContentHeight += 20.0f;
            
            float rightX = ImGui::GetWindowWidth() - rightColumnWidth;
            float rightY = rowMin.y + (rowHeight - rightContentHeight) / 2.0f;
            ImGui::SetCursorScreenPos(ImVec2(rightX, rightY));

            ImGui::BeginGroup();
            if (achievement.IsHidden) {   // was bIsHidden
                ImGui::TextColored(ImVec4(0.9f, 0.5f, 0.2f, 1), "Hidden");
            }
            switch (achievement.UnlockState) {
            case UnlockState::Unlocked:
                ImGui::TextColored(ImVec4(0, 0.9f, 0, 1), "Unlocked");
                break;
            case UnlockState::Unlocking:
                ImGui::TextColored(ImVec4(0.9f, 0.9f, 0, 1), "Unlocking...");
                break;
            case UnlockState::Locked:
                if (ImGui::Button("Unlock", ImVec2(70, 28))) {
                    Overlay::unlockAchievement(&achievement);
                }
                break;
            }
            ImGui::EndGroup();

            // Right-click context menu
            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                ImGui::OpenPopup("AchievementContextMenu");
            }
            if (ImGui::BeginPopup("AchievementContextMenu")) {
                if (ImGui::Selectable("Copy Achievement ID")) {
                    CopyToClipboard(achievement.AchievementId);
                }
                ImGui::EndPopup();
            }

            ImGui::PopID();
        }
        ImGui::SetCursorScreenPos(ImVec2(startPos.x, startPos.y + (endIdx - startIdx) * rowHeight));
        
        if (usePagination && totalFiltered > itemsPerPage) {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            int totalPages = (totalFiltered + itemsPerPage - 1) / itemsPerPage;
            ImGui::BeginGroup();
            ImGui::Text("Page %d / %d", currentPage + 1, totalPages);
            ImGui::SameLine();
            if (ImGui::Button("<<")) currentPage = 0;
            ImGui::SameLine();
            if (ImGui::Button("<") && currentPage > 0) currentPage--;
            ImGui::SameLine();
            if (ImGui::Button(">") && currentPage < totalPages - 1) currentPage++;
            ImGui::SameLine();
            if (ImGui::Button(">>")) currentPage = totalPages - 1;
            ImGui::SameLine();
            ImGui::Text("  (%d achievements)", totalFiltered);
            ImGui::EndGroup();
        }
    }
    ImGui::EndChild();
    ImGui::End();
}

} // namespace AchievementManagerUI