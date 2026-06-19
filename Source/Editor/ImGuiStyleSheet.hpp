#pragma once

#include "Engine/ThirdParty/imgui/imgui.h"

namespace Editor
{
struct ImGuiButtonStyle
{
    ImVec4 button;
    ImVec4 hovered;
    ImVec4 active;
    float frameRounding = 0.0f;
    float widthScale = 1.0f;
};

class ScopedImGuiButtonStyle
{
public:
    explicit ScopedImGuiButtonStyle(const ImGuiButtonStyle& style)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, style.frameRounding);
        ImGui::PushStyleColor(ImGuiCol_Button, style.button);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, style.hovered);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, style.active);
    }

    ~ScopedImGuiButtonStyle()
    {
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
    }

    ScopedImGuiButtonStyle(const ScopedImGuiButtonStyle&) = delete;
    ScopedImGuiButtonStyle& operator=(const ScopedImGuiButtonStyle&) = delete;
};

class ImGuiStyleSheet
{
public:
    static ImGuiButtonStyle AccentToggleButton(bool active)
    {
        return {
            active ? ImVec4(0.22f, 0.38f, 0.55f, 1.0f) : ImVec4(0.28f, 0.32f, 0.42f, 1.0f),
            active ? ImVec4(0.30f, 0.50f, 0.72f, 1.0f) : ImVec4(0.38f, 0.43f, 0.56f, 1.0f),
            ImVec4(0.18f, 0.50f, 0.82f, 1.0f),
            7.0f,
            1.35f
        };
    }

    static ImVec2 FrameButtonSize(const ImGuiButtonStyle& style)
    {
        return ImVec2(ImGui::GetFrameHeight() * style.widthScale, 0.0f);
    }
};
} // namespace Editor
