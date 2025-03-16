#include "GameObjectInspector.hpp"
#include "Core/Component/Component.hpp"
#include "EditorState.hpp"
#include "ThirdParty/imgui/imgui.h"

namespace Editor
{
char GameObjectInspector::_register = InspectorRegistry::Register<GameObjectInspector, GameObject>();

void GameObjectInspector::PreventNegativeZero(float& val)
{
    if (val == -0.0f)
        val = 0.0f;
}

void GameObjectInspector::DrawInspector(GameEditor& editor)
{
    ImGui::BeginMenuBar();
    // Create Component
    if (ImGui::BeginMenu("Create Component"))
    {
        auto componentNames = ObjectRegistry::GetComponentTypeNames();
        std::sort(componentNames.begin(), componentNames.end(), [](auto& l, auto& r) { return l < r; });
        for (auto& componentName : ObjectRegistry::GetComponentTypeNames())
        {
            if (ImGui::MenuItem(componentName.c_str()))
                target->AddComponent(componentName);
        }

        ImGui::EndMenu();
    }
    ImGui::EndMenuBar();

    ImGui::Text("%s", target->GetUUID().ToString().c_str());

    // Object information
    ImGui::SeparatorText("Object Information");
    auto& name = target->GetName();
    char cname[1024];
    strcpy(cname, name.data());
    bool enabled = target->IsEnabled();
    if (ImGui::Checkbox("##Enable Box", &enabled))
    {
        target->SetEnable(enabled);
    }

    ImGui::SameLine();

    if (ImGui::InputText("Name", cname, 1024))
    {
        target->SetName(cname);
    }

    glm::vec3 pos = target->GetLocalPosition();
    if (ImGui::DragFloat3("Position", &pos[0]))
    {
        target->SetLocalPosition(pos);
    }

    auto rotation = target->GetEuluerAngles();
    auto degree = glm::degrees(rotation);
    if (ImGui::DragFloat3("rotation", &degree[0]))
    {
        target->SetEulerAngles(glm::radians(degree));
    }

    auto scale = target->GetLocalScale();
    if (ImGui::DragFloat3("scale", &scale[0]))
    {
        target->SetScale(scale);
    }

    // Show prefab
    auto prefab = target->GetPrefab();
    if (prefab)
    {
        ImGui::SeparatorText("Prefab");
        if (ImGui::Button("Reset To Prefab"))
        {
            target->ResetToPrefab();
        }
    }

    ImGui::SeparatorText("Components");
    int enableCheckBoxID = 0;
    bool popupTriggered = false;
    for (auto& co : target->GetComponents())
    {
        ImGui::PushID(enableCheckBoxID++);
        auto& c = *co;
        bool cEnabled = c.IsEnabled();
        if (ImGui::Checkbox("##Enable", &cEnabled))
        {
            if (cEnabled)
                c.Enable();
            else
                c.Disable();
        }
        ImGui::SameLine();
        // ImGui::SeparatorText(c.GetName().c_str());
        bool showAsSelected = contextComponent != nullptr && (contextComponent == co.get() ||
                                                              EditorState::GetMainSelectedObject() == contextComponent);
        ImGuiTreeNodeFlags treeNodeFlags = showAsSelected ? ImGuiTreeNodeFlags_Selected : 0;
        bool expandComponent = ImGui::TreeNodeEx(c.GetName().c_str(), treeNodeFlags);
        GUI::DragDropSource(c.GetName().c_str(), &c);
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && ImGui::IsItemHovered())
        {
            if (!popupTriggered)
            {
                popupTriggered = true;
                contextComponent = co.get();
            }
        }

        if (expandComponent)
        {
            auto inspector = InspectorRegistry::GetInspector(c);
            inspector->OnEnable(c);
            inspector->DrawInspector(editor);
            ImGui::TreePop();
        }

        ImGui::PopID();
    }
    if (popupTriggered)
        ImGui::OpenPopup("Component Context");

    if (ImGui::BeginPopup("Component Context"))
    {
        if (ImGui::MenuItem("Delete"))
        {
            target->RemoveComponent(contextComponent);
            contextComponent = nullptr;
        }
        ImGui::EndPopup();
    }

    if (!ImGui::IsPopupOpen("Component Context") && !popupTriggered)
    {
        contextComponent = nullptr;
    }
}

} // namespace Editor
