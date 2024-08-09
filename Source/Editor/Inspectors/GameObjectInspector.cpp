#include "../EditorState.hpp"
#include "Core/Component/Camera.hpp"
#include "Core/Component/Light.hpp"
#include "Core/Component/LightFieldProbes.hpp"
#include "Core/Component/LuaScript.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "Core/Component/PhysicsBody.hpp"
#include "Core/Component/SceneEnvironment.hpp"
#include "Core/GameObject.hpp"
#include "Core/Scene/Scene.hpp"
#include "GamePlay/Component/PlayerController.hpp"
#include "Inspector.hpp"
#include "Rendering/FrameGraph/FrameGraph.hpp"
#include "ThirdParty/imgui/imgui.h"
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Editor
{
class GameObjectInspector : public Inspector<GameObject>
{
    inline void PreventNegativeZero(float& val)
    {
        if (val == -0.0f)
            val = 0.0f;
    }

    void DrawInspector(GameEditor& editor) override
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
            if (ImGui::MenuItem("LuaScript"))
                target->AddComponent<LuaScript>();
            if (ImGui::MenuItem("PlayerController"))
                target->AddComponent<PlayerController>();
            if (ImGui::MenuItem("LightFieldProbes"))
                target->AddComponent<LightFieldProbes>();

            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();

        // object information
        ImGui::Separator();
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

        auto scale = target->GetScale();
        if (ImGui::DragFloat3("scale", &scale[0]))
        {
            target->SetScale(scale);
        }

        // Components
        int enableCheckBoxID = 0;
        Component* removeThis = nullptr;
        for (auto& co : target->GetComponents())
        {
            ImGui::PushID(enableCheckBoxID++);
            auto& c = *co;
            ImGui::Separator();
            bool cEnabled = c.IsEnabled();
            if (ImGui::Checkbox("##Enable", &cEnabled))
            {
                if (cEnabled)
                    c.Enable();
                else
                    c.Disable();
            }
            ImGui::SameLine();
            ImGui::Text("%s", c.GetName().c_str());
            ImGui::SameLine();
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

private:
    static char _register;
    glm::vec3 point, axis;
    float angle;
};

char GameObjectInspector::_register = InspectorRegistry::Register<GameObjectInspector, GameObject>();

} // namespace Editor
