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
            if (ImGui::MenuItem("Light"))
            {
                auto light = target->AddComponent<Light>();
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
            else if (typeid(c) == typeid(MeshRenderer))
            {
                MeshRenderer& meshRenderer = static_cast<MeshRenderer&>(c);
                Mesh* mesh = meshRenderer.GetMesh();

                std::string meshGUIID = "emtpy";
                if (mesh)
                    meshGUIID = mesh->GetName();

                ImGui::Text("Mesh: ");
                ImGui::SameLine();

                bool bp = ImGui::Button(meshGUIID.c_str());
                if (ImGui::BeginDragDropTarget())
                {
                    const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("object");
                    if (payload && payload->IsDelivery())
                    {
                        Object& obj = **(Object**)payload->Data;
                        if (typeid(obj) == typeid(Mesh))
                        {
                            Mesh* mesh = static_cast<Mesh*>(&obj);

                            meshRenderer.SetMesh(mesh);
                            mesh = meshRenderer.GetMesh();
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
                else if (bp)
                {
                    EditorState::selectedObject = mesh;
                }
                // show materials
                ImGui::Text("Materials: ");
                std::vector<Material*> mats = meshRenderer.GetMaterials();
                for (int i = 0; i < mats.size(); ++i)
                {
                    Material* mat = mats[i];
                    std::string buttonID;
                    if (mat)
                        buttonID = fmt::format("{}: {}", i, mats[i]->GetName());
                    else
                        buttonID = fmt::format("{}##{}", "emtpy", i);

                    if (ImGui::Button(buttonID.c_str()))
                    {
                        EditorState::selectedObject = mats[i];
                    };

                    if (ImGui::BeginDragDropTarget())
                    {
                        const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("object");
                        if (payload && payload->IsDelivery())
                        {
                            Object& obj = **(Object**)payload->Data;
                            if (typeid(obj) == typeid(Material))
                            {
                                mats[i] = static_cast<Material*>(&obj);
                                meshRenderer.SetMaterials(mats);
                            }
                        }
                        ImGui::EndDragDropTarget();
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
