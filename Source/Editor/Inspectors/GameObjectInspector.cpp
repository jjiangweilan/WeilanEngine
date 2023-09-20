#include "Core/Component/Camera.hpp"
#include "Core/Component/Light.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "Core/GameObject.hpp"
#include "Inspector.hpp"
#include "ThirdParty/imgui/imgui.h"

namespace Engine::Editor
{
class GameObjectInspector : public Inspector<GameObject>
{
public:
    void DrawInspector() override
    {
        ImGui::BeginMenuBar();
        // Create Component
        if (ImGui::BeginMenu("Create Component"))
        {
            if (ImGui::MenuItem("Camera"))
            {
                auto camera = target->AddComponent<Camera>();
            }
            if (ImGui::MenuItem("MeshRenderer"))
            {
                auto meshRenderer = target->AddComponent<MeshRenderer>();
            }

            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();

        // object information
        ImGui::Separator();
        auto& name = target->GetName();
        char cname[1024];
        strcpy(cname, name.data());
        if (ImGui::InputText("Name", cname, 1024))
        {
            target->SetName(cname);
        }

        // Components
        for (auto& c : target->GetComponents())
        {
            Transform* transform = target->GetTransform();
            if (c->GetName() == "Transform")
            {
                ImGui::Separator();
                ImGui::Text("Transform");
                auto pos = transform->GetPosition();
                if (ImGui::InputFloat3("Position", &pos[0]))
                {
                    transform->SetPosition(pos);
                }

                auto rotation = transform->GetRotation();
                if (ImGui::InputFloat3("rotation", &rotation[0]))
                {
                    transform->SetRotation(rotation);
                }
                continue;
            }
            else if (c->GetName() == "Light")
            {
                ImGui::Separator();
                ImGui::Text("Transform");
                Light* light = static_cast<Light*>(c.get());
                float intensity = light->GetIntensity();
                ImGui::DragFloat("intensity", &intensity);
                light->SetIntensity(intensity);
                continue;
            }
            else if (c->GetName() == "MeshRenderer")
            {
                ImGui::Separator();
                ImGui::Text("MeshRenderer");
            }
        }
    }

private:
    static char _register;
};

char GameObjectInspector::_register = InspectorRegistry::Register<GameObjectInspector, GameObject>();

} // namespace Engine::Editor
