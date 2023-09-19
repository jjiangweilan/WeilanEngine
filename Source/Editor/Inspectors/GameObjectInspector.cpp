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

        if (ImGui::BeginMenu("Create Component"))
        {
            if (ImGui::MenuItem("MeshRenderer"))
            {
                auto meshRenderer = target->AddComponent<MeshRenderer>();
            }

            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();

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
