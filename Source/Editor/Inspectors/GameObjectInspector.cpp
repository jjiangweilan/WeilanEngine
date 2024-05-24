#include "../EditorState.hpp"
#include "Core/Component/Camera.hpp"
#include "Core/Component/Light.hpp"
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
public:
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

        auto pos = target->GetPosition();
        if (ImGui::DragFloat3("Position", &pos[0]))
        {
            target->SetPosition(pos);
        }

        auto rotation = target->GetEuluerAngles();
        auto degree = glm::degrees(rotation);
        auto newDegree = degree;
        if (ImGui::DragFloat3("rotation", &newDegree[0]))
        {
            auto delta = newDegree - degree;
            auto radians = glm::radians(delta);
            target->SetEulerAngles(glm::radians(newDegree));
            // float length = glm::length(radians);
            // if (length != 0)
            //     target->Rotate(length, glm::normalize(radians), RotationCoordinate::Self);
        }

        auto scale = target->GetScale();
        if (ImGui::DragFloat3("scale", &scale[0]))
        {
            target->SetScale(scale);
        }

        // Components
        for (auto& co : target->GetComponents())
        {
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

            auto inspector = InspectorRegistry::GetInspector(c);
            inspector->OnEnable(c);
            inspector->DrawInspector(editor);
        }
    }

private:
    static char _register;
};

char GameObjectInspector::_register = InspectorRegistry::Register<GameObjectInspector, GameObject>();

} // namespace Editor
