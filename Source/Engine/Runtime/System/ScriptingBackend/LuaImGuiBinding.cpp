#include "Engine/WeilanEngineAPI.hpp"
#include "Engine/ThirdParty/imgui/imgui.h"

#include <cstddef>

namespace
{
bool HasImGuiContext()
{
    return ImGui::GetCurrentContext() != nullptr;
}
} // namespace

extern "C"
{
WEILAN_ENGINE_API bool WeilanImGui_Begin(const char* name, bool* open, int flags)
{
    if (!HasImGuiContext())
        return false;

    return ImGui::Begin(name, open, flags);
}

WEILAN_ENGINE_API void WeilanImGui_End()
{
    if (!HasImGuiContext())
        return;

    ImGui::End();
}

WEILAN_ENGINE_API void WeilanImGui_Text(const char* text)
{
    if (!HasImGuiContext())
        return;

    ImGui::TextUnformatted(text);
}

WEILAN_ENGINE_API bool WeilanImGui_Button(const char* label, float width, float height)
{
    if (!HasImGuiContext())
        return false;

    return ImGui::Button(label, ImVec2(width, height));
}

WEILAN_ENGINE_API bool WeilanImGui_Checkbox(const char* label, bool* value)
{
    if (!HasImGuiContext())
        return false;

    return ImGui::Checkbox(label, value);
}

WEILAN_ENGINE_API bool WeilanImGui_SliderFloat(const char* label, float* value, float minValue, float maxValue)
{
    if (!HasImGuiContext())
        return false;

    return ImGui::SliderFloat(label, value, minValue, maxValue);
}

WEILAN_ENGINE_API bool WeilanImGui_DragFloat3(const char* label, float* values, float speed, float minValue, float maxValue)
{
    if (!HasImGuiContext())
        return false;

    return ImGui::DragFloat3(label, values, speed, minValue, maxValue);
}

WEILAN_ENGINE_API bool WeilanImGui_InputText(const char* label, char* buffer, size_t bufferSize, int flags)
{
    if (!HasImGuiContext())
        return false;

    return ImGui::InputText(label, buffer, bufferSize, flags);
}

WEILAN_ENGINE_API void WeilanImGui_SameLine(float offsetFromStartX, float spacing)
{
    if (!HasImGuiContext())
        return;

    ImGui::SameLine(offsetFromStartX, spacing);
}

WEILAN_ENGINE_API void WeilanImGui_Separator()
{
    if (!HasImGuiContext())
        return;

    ImGui::Separator();
}

WEILAN_ENGINE_API void WeilanImGui_NewLine()
{
    if (!HasImGuiContext())
        return;

    ImGui::NewLine();
}

WEILAN_ENGINE_API void WeilanImGui_Spacing()
{
    if (!HasImGuiContext())
        return;

    ImGui::Spacing();
}

WEILAN_ENGINE_API void WeilanImGui_SetNextWindowSize(float width, float height, int condition)
{
    if (!HasImGuiContext())
        return;

    ImGui::SetNextWindowSize(ImVec2(width, height), condition);
}

WEILAN_ENGINE_API void WeilanImGui_SetNextWindowPos(float x, float y, int condition)
{
    if (!HasImGuiContext())
        return;

    ImGui::SetNextWindowPos(ImVec2(x, y), condition);
}

WEILAN_ENGINE_API bool WeilanImGui_CollapsingHeader(const char* label, int flags)
{
    if (!HasImGuiContext())
        return false;

    return ImGui::CollapsingHeader(label, flags);
}

WEILAN_ENGINE_API bool WeilanImGui_TreeNode(const char* label)
{
    if (!HasImGuiContext())
        return false;

    return ImGui::TreeNode(label);
}

WEILAN_ENGINE_API void WeilanImGui_TreePop()
{
    if (!HasImGuiContext())
        return;

    ImGui::TreePop();
}

WEILAN_ENGINE_API void WeilanImGui_GetContentRegionAvail(float* width, float* height)
{
    if (!HasImGuiContext())
    {
        *width = 0.0f;
        *height = 0.0f;
        return;
    }

    ImVec2 size = ImGui::GetContentRegionAvail();
    *width = size.x;
    *height = size.y;
}

WEILAN_ENGINE_API bool WeilanImGui_IsWindowHovered(int flags)
{
    if (!HasImGuiContext())
        return false;

    return ImGui::IsWindowHovered(flags);
}

WEILAN_ENGINE_API bool WeilanImGui_IsItemHovered(int flags)
{
    if (!HasImGuiContext())
        return false;

    return ImGui::IsItemHovered(flags);
}
}
