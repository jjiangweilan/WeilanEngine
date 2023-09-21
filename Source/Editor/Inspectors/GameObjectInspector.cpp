#include "../EditorState.hpp"
#include "Core/Component/Camera.hpp"
#include "Core/Component/Light.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "Core/GameObject.hpp"
#include "Core/Scene/Scene.hpp"
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
        for (auto& co : target->GetComponents())
        {
            auto& c = *co;
            ImGui::Separator();
            ImGui::Text("%s", c.GetName().c_str());

            Transform* transform = target->GetTransform();
            if (typeid(c) == typeid(Transform))
            {
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
            else if (typeid(c) == typeid(Light))
            {
                Light* light = static_cast<Light*>(&c);
                float intensity = light->GetIntensity();
                ImGui::DragFloat("intensity", &intensity);
                light->SetIntensity(intensity);
                continue;
            }
            else if (typeid(c) == typeid(Camera))
            {
                Camera& camera = static_cast<Camera&>(c);
                if (ImGui::Button("Set as main camera"))
                {
                    if (EditorState::activeScene)
                    {
                        EditorState::activeScene->SetMainCamera(&camera);
                    }
                }
            }
        }
    }

private:
    static char _register;
};

char GameObjectInspector::_register = InspectorRegistry::Register<GameObjectInspector, GameObject>();

} // namespace Engine::Editor
