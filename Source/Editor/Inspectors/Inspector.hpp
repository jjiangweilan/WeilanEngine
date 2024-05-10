#pragma once
#include "Core/Asset.hpp"
#include "Core/Component/Component.hpp"
#include "Core/Object.hpp"
#include "InspectorRegistry.hpp"
#include "ThirdParty/imgui/imgui.h"
#include <concepts>
namespace Editor
{
template <typename T>
concept HasGetName = requires(T t) {
    { t.GetName().c_str() };
};
class GameEditor;
// all inspector should be stateless, same inspector instance is reused for different object in GameEditor
class InspectorBase
{
public:
    virtual ~InspectorBase(){};
    virtual void DrawInspector(GameEditor& editor) = 0;
    virtual void OnEnable(Object& obj) = 0;
    virtual Object* GetTarget() = 0;
};

template <class T>
class Inspector : public InspectorBase
{
public:
    void OnEnable(Object& obj) override
    {
        target = static_cast<T*>(&obj);
    }
    void DrawInspector(GameEditor& editor) override
    {
        // default inspector
        ImGui::Button(((Object*)target)->GetUUID().ToString().c_str());

        if (ImGui::BeginDragDropSource())
        { 
            ImGui::SetDragDropPayload("object", &target, sizeof(void*));
            if constexpr (HasGetName<T>)
                ImGui::Text("%s", target->GetName().c_str());
            else
                ImGui::Text("%s", target->GetUUID().ToString().c_str());
            ImGui::EndDragDropSource();
        }

        if (Asset* asset = dynamic_cast<Asset*>(target))
        {
            auto nameStr = asset->GetName();
            char name[256];
            strcpy(name, nameStr.c_str());
            if (ImGui::InputText("name", name, 256))
            {
                asset->SetName(name);
            }
        }
    };

    Object* GetTarget() override
    {
        return target;
    }

protected:
    T* target;
};
} // namespace Editor
