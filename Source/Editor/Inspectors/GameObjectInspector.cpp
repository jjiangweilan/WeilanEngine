#include "GameObjectInspector.hpp"

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
        if (ImGui::MenuItem("Camera"))
            target->AddComponent<Camera>();
        if (ImGui::MenuItem("MeshRenderer"))
            target->AddComponent<MeshRenderer>();
        if (ImGui::MenuItem("Light"))
            target->AddComponent<Light>();
        if (ImGui::MenuItem("SceneEnvironment"))
            target->AddComponent<SceneEnvironment>();
        if (ImGui::MenuItem("PhysicsBody"))
            target->AddComponent<PhysicsBody>();
        if (ImGui::MenuItem("GameScript"))
            target->AddComponent<GameScript>();
        if (ImGui::MenuItem("PlayerController"))
            target->AddComponent<PlayerController>();
        if (ImGui::MenuItem("LightFieldProbes"))
            target->AddComponent<LightFieldProbes>();
        if (ImGui::MenuItem("GrassSurface"))
            target->AddComponent<GrassSurface>();
        if (ImGui::MenuItem("AnimationPlayer"))
            target->AddComponent<AnimationPlayer>();
        if (ImGui::MenuItem("Cloud"))
            target->AddComponent<Cloud>();
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

    // Components
    int enableCheckBoxID = 0;
    Component* removeThis = nullptr;
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
        ImGui::SeparatorText(c.GetName().c_str());
        if (ImGui::Button("remove!"))
        {
            removeThis = co.get();
        }

        auto inspector = InspectorRegistry::GetInspector(c);
        inspector->OnEnable(c);
        inspector->DrawInspector(editor);
        ImGui::PopID();
    }

    if (removeThis != nullptr)
        target->RemoveComponent(removeThis);
}

} // namespace Editor
