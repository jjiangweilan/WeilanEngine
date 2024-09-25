#pragma once
#include "Core/Asset.hpp"
#include "Core/Object.hpp"
#include "EditorState.hpp"
#include "ThirdParty/imgui/imgui.h"
#include <concepts>
#include <string_view>

namespace Editor
{
class GUI
{
public:
    template <std::derived_from<Object> T>
    static bool ObjectField(std::string_view name, T*& curr)
    {
        ImGui::PushID(0);
        ImGui::Text("%s: ", name.data());
        ImGui::SameLine();
        std::string buttonName = "null";
        Asset* asset = dynamic_cast<Asset*>(curr);
        if (asset)
        {
            buttonName = asset->GetName();
        }
        else if (curr != nullptr)
        {
            buttonName = curr->GetUUID().ToString().c_str();
        }
        if (ImGui::Button(curr == nullptr ? "null" : buttonName.c_str()))
        {
            EditorState::SelectObject(curr ? curr->GetSRef() : nullptr);
        }

        if (ImGui::BeginDragDropTarget())
        {
            auto payload = ImGui::AcceptDragDropPayload("object");
            if (payload && payload->IsDelivery())
            {
                if (T* c = dynamic_cast<T*>(*(Object**)payload->Data))
                {
                    curr = c;
                    return true;
                }
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::PopID();

        return false;
    }
};
} // namespace Editor
