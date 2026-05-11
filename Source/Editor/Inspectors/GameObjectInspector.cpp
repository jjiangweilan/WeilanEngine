#include "GameObjectInspector.hpp"
#include "Editor/EditorState.hpp"
#include "Editor/UndoManager.hpp"
#include "Engine/Runtime/Object/Component/Component.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetDatabase.hpp"
#include "Engine/ThirdParty/imgui/imgui.h"

namespace Editor
{
namespace
{
bool InspectorUndoInputEvent()
{
    ImGuiIO& io = ImGui::GetIO();
    return ImGui::IsAnyItemActive() || ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
           ImGui::IsMouseClicked(ImGuiMouseButton_Right) || io.InputQueueCharacters.Size > 0 ||
           ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Space) ||
           ImGui::IsKeyPressed(ImGuiKey_Backspace) || ImGui::IsKeyPressed(ImGuiKey_Delete);
}

} // namespace

char GameObjectInspector::_register = InspectorRegistry::Register<GameObjectInspector, GameObject>();

void GameObjectInspector::PreventNegativeZero(float& val)
{
    if (val == -0.0f)
        val = 0.0f;
}

void GameObjectInspector::DrawInspector(GameEditor& editor)
{
    auto& undoManager = EditorState::GetUndoManager();

    ImGui::BeginMenuBar();
    // Create Component

    if (ImGui::BeginMenu("Create Component"))
    {
        auto componentNames = ObjectRegistry::GetComponentTypeNames();
        std::sort(componentNames.begin(), componentNames.end(), [](auto& l, auto& r)
                  { return l < r; });

        int selected = -1;
        int firstItem = -1;
        if (EditorGUI::SearchableMenuItems(componentNames, searchComponent, selected, firstItem))
        {
            std::string componentName = componentNames[selected];
            undoManager.TrackGameObject(target.Get());
            target->AddComponent(componentName);
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Enter) && firstItem != -1)
        {
            std::string componentName = componentNames[firstItem];
            undoManager.TrackGameObject(target.Get());
            target->AddComponent(componentName);

            ImGui::CloseCurrentPopup();
        }

        ImGui::EndMenu();
    }
    ImGui::EndMenuBar();

    EditorGUI::Text("UUID", target->GetUUID().ToString().c_str());

    // Object information
    EditorGUI::SeparatorTextLabeled("Object Information");
    auto& name = target->GetName();
    char cname[1024];
    strcpy(cname, name.data());
    bool enabled = target->IsEnabled() || target->WantsTobeEnabled(); // wants to be enabled is used for prefab inspector
    if (ImGui::Checkbox("##Enable Box", &enabled))
    {
        undoManager.TrackGameObject(target.Get());
        target->SetEnable(enabled);
    }

    ImGui::SameLine();

    bool nameChanged = EditorGUI::InputTextLabeled("Name", cname, 1024);
    if (nameChanged)
    {
        undoManager.TrackGameObject(target.Get());
        target->SetName(cname);
    }

    glm::vec3 pos = target->GetLocalPosition();
    bool positionChanged = ImGui::DragFloat3("Position", &pos[0]);
    if (positionChanged)
    {
        undoManager.TrackGameObjectHierarchyPlacement(target.Get());
        target->SetLocalPosition(pos);
    }

    auto rotation = target->GetEuluerAngles();
    auto degree = glm::degrees(rotation);
    bool rotationChanged = ImGui::DragFloat3("rotation", &degree[0]);
    if (rotationChanged)
    {
        undoManager.TrackGameObjectHierarchyPlacement(target.Get());
        target->SetEulerAngles(glm::radians(degree));
    }

    auto scale = target->GetLocalScale();
    bool scaleChanged = ImGui::DragFloat3("scale", &scale[0]);
    if (scaleChanged)
    {
        undoManager.TrackGameObjectHierarchyPlacement(target.Get());
        target->SetScale(scale);
    }

    bool resetToPrefab = false;
    bool applyToPrefab = false;

    // Show prefab
    auto prefab = target->GetPrefab();
    if (prefab)
    {
        ImGui::SeparatorText("Prefab");
        if (ImGui::Button("Reset To Prefab"))
        {
            resetToPrefab = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Apply To Prefab"))
        {
            applyToPrefab = true;
        }
    }

    ImGui::SeparatorText("Components");
    int enableCheckBoxID = 0;
    bool popupTriggered = false;
    int currentComponentIdx = 0;
    GameObject* targetGO = target;
    for (auto& co : targetGO->GetComponents())
    {
        ImGui::PushID(enableCheckBoxID++);
        if (co)
        {
            auto& c = *co;
            bool cEnabled = c.IsEnabled();
            if (ImGui::Checkbox("##Enable", &cEnabled))
            {
                undoManager.TrackGameObject(targetGO);
                if (cEnabled)
                    c.Enable();
                else
                    c.Disable();
            }
            ImGui::SameLine();
            // ImGui::SeparatorText(c.GetName().c_str());
            bool showAsSelected =
                contextComponent != nullptr &&
                (contextComponent == co || EditorState::GetMainSelectedObject() == contextComponent);
            ImGuiTreeNodeFlags treeNodeFlags = showAsSelected ? ImGuiTreeNodeFlags_Selected : 0;
            bool expandComponent = ImGui::TreeNodeEx(c.GetName().c_str(), treeNodeFlags);
            EditorGUI::DragDropSource(c.GetName().c_str(), &c);
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && ImGui::IsItemHovered())
            {
                if (!popupTriggered)
                {
                    popupTriggered = true;
                    contextComponent = co;
                    contextComponentIdx = currentComponentIdx;
                }
            }

            if (expandComponent)
            {
                auto inspector = InspectorRegistry::GetInspector(c);
                inspector->OnEnable(c);
                if (InspectorUndoInputEvent())
                    undoManager.TrackGameObject(targetGO);
                inspector->DrawInspector(editor);
                ImGui::TreePop();
            }
        }
        else
        {
            std::string nullObjectName = "null - component type is probably removed";

            ImGui::PushStyleColor(ImGuiCol_Button, {0, 0, 0, 0});
            ImGui::Button(nullObjectName.c_str());
            ImGui::PopStyleColor(1);
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && ImGui::IsItemHovered())
            {
                if (!popupTriggered)
                {
                    popupTriggered = true;
                    contextComponent = co;
                    contextComponentIdx = currentComponentIdx;
                }
            }
        }

        currentComponentIdx += 1;
        ImGui::PopID();
    }
    if (popupTriggered)
        ImGui::OpenPopup("Component Context");

    if (ImGui::BeginPopup("Component Context"))
    {
        if (ImGui::MenuItem("Delete"))
        {
            int componentIndex = contextComponentIdx;
            undoManager.TrackGameObject(target.Get());
            target->RemoveComponentByIndex(componentIndex);
            contextComponent = nullptr;
            contextComponentIdx = -1;
        }
        ImGui::EndPopup();
    }

    if (!ImGui::IsPopupOpen("Component Context") && !popupTriggered)
    {
        contextComponent = nullptr;
    }

    if (resetToPrefab)
    {
        undoManager.TrackGameObject(target.Get());
        target->ResetToPrefab();
    }

    if (applyToPrefab)
    {
        undoManager.TrackAsset(target->GetPrefab().Get());
        target->ApplyToPrefab();
    }
}

} // namespace Editor
