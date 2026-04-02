#include "pch.h"
#include "achievement_manager_ui.h"
#include <Overlay.h>
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace AchievementManagerUI {

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

static char searchBuffer[128] = "";
static int filterMode = 0; // 0 = All, 1 = Locked only, 2 = Unlocked only

void InitImGui(void* pWindow, ID3D11Device* pD3D11Device, ID3D11DeviceContext* pContext) {
    ImGui::CreateContext();
    ImGuiStyle& style = ImGui::GetStyle();

    // Modern color scheme
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
    style.ScrollbarRounding= 6.0f;
    style.GrabRounding    = 4.0f;
    style.WindowRounding  = 10.0f;

    style.WindowPadding   = ImVec2(12, 12);
    style.ItemSpacing     = ImVec2(8, 8);
    style.ItemInnerSpacing= ImVec2(6, 6);

    auto& io = ImGui::GetIO();
    io.IniFilename = NULL;
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
                Logger::ovrly("ImGui: Loaded font: %s", path);
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
        Logger::ovrly("ImGui: Using default font");
    }

    ImGui_ImplWin32_Init(pWindow);
    ImGui_ImplDX11_Init(pD3D11Device, pContext);
    Logger::ovrly("ImGui: Successfully initialized");
}

void ShutdownImGui() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    Logger::ovrly("ImGui: Shutdown");
}

void FitTextToWindow(ImFont* font, const ImVec4 color, const char* text) {
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

static bool containsIgnoreCase(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    auto it = std::search(haystack.begin(), haystack.end(),
        needle.begin(), needle.end(),
        [](char ch1, char ch2) { return std::toupper(ch1) == std::toupper(ch2); });
    return it != haystack.end();
}

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

    // Statistics summary (no emoji)
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
        ImGui::Spacing();
    }

    // Filter controls
    ImGui::BeginGroup();
    ImGui::PushItemWidth(150);
    if (ImGui::InputTextWithHint("##search", "Filter...", searchBuffer, sizeof(searchBuffer))) {}
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button("All")) filterMode = 0;
    ImGui::SameLine();
    if (ImGui::Button("Locked")) filterMode = 1;
    ImGui::SameLine();
    if (ImGui::Button("Unlocked")) filterMode = 2;
    if (strlen(searchBuffer) > 0 || filterMode != 0) {
        ImGui::SameLine();
        if (ImGui::Button("Clear")) {
            searchBuffer[0] = '\0';
            filterMode = 0;
        }
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

        float rowHeight = 70.0f;
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 startPos = ImGui::GetCursorScreenPos();

        for (size_t row = 0; row < filteredIndices.size(); row++) {
            int idx = filteredIndices[row];
            auto& achievement = Overlay::achievements->at(idx);
            ImGui::PushID(idx);

            // Row rectangle
            ImVec2 rowMin = ImVec2(startPos.x, startPos.y + row * rowHeight);
            ImVec2 rowMax = ImVec2(rowMin.x + ImGui::GetWindowWidth(), rowMin.y + rowHeight);
            ImU32 bgColor = (row % 2 == 0) ? IM_COL32(30, 30, 35, 230) : IM_COL32(25, 25, 30, 230);
            drawList->AddRectFilled(rowMin, rowMax, bgColor, 4.0f);

            // Set cursor to the start of this row
            ImGui::SetCursorScreenPos(rowMin);

            // Star icon (left, centered)
            float iconHeight = 50.0f;
            float iconYOffset = (rowHeight - iconHeight) / 2.0f;
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + iconYOffset);
            ImGui::PushFont(bigFont);
            ImGui::TextUnformatted("★");
            ImGui::PopFont();
            ImGui::SameLine();

            // Text block width
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

            // Right column: centered vertically using absolute positioning
            float rightContentHeight = 0.0f;
            if (achievement.IsHidden) rightContentHeight += 20.0f;
            if (achievement.UnlockState == UnlockState::Locked) rightContentHeight += 28.0f;
            else rightContentHeight += 20.0f;
            
            float rightX = ImGui::GetWindowWidth() - rightColumnWidth;
            float rightY = rowMin.y + (rowHeight - rightContentHeight) / 2.0f;
            ImGui::SetCursorScreenPos(ImVec2(rightX, rightY));

            ImGui::BeginGroup();
            if (achievement.IsHidden) {
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

            ImGui::PopID();
        }
        // Advance cursor after drawing all rows
        ImGui::SetCursorScreenPos(ImVec2(startPos.x, startPos.y + filteredIndices.size() * rowHeight));
    }
    ImGui::EndChild();
    ImGui::End();
}

void DrawInitPopup() {
    static auto grayCol = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
    static auto greenCol = ImVec4(0.0f, 0.85f, 0.0f, 1.0f);

    ImGui::SetNextWindowSize(ImVec2(560, 140), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.10f, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_Border, greenCol);
    ImGui::Begin("InitPopup", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
    ImGui::PopStyleColor(2);

    ImGui::PushFont(bigFont);
    ImGui::TextColored(grayCol, "Epic Achievements Unlocker is");
    ImGui::SameLine();
    ImGui::TextColored(greenCol, "online");
    ImGui::PopFont();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushFont(instructionFont);
    ImGui::PushTextWrapPos(540.0f);
    ImGui::TextColored(grayCol, "Press [Shift] + [F5] to open the achievement panel.");
    ImGui::PopTextWrapPos();
    ImGui::PopFont();

    ImGui::End();
}

} // namespace AchievementManagerUI