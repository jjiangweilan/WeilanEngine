#pragma once
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
        if (ImGui::Button(name.data()))
        {
            EditorState::selectedObject = curr ? curr->GetSRef() : nullptr;
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

        return false;
    }
};
} // namespace Editor
