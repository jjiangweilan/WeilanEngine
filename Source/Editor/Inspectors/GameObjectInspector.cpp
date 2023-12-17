#include "../EditorState.hpp"
#include "Core/Component/Camera.hpp"
#include "Core/Component/Light.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "Core/GameObject.hpp"
#include "Core/Scene/Scene.hpp"
#include "Inspector.hpp"
#include "Rendering/FrameGraph/FrameGraph.hpp"
#include "ThirdParty/imgui/imgui.h"
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
namespace Engine::Editor
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

        auto pos = target->GetPosition();
        if (ImGui::DragFloat3("Position", &pos[0]))
        {
            target->SetPosition(pos);
        }

        auto rotation = glm::eulerAngles(target->GetRotation());
        auto degree = glm::degrees(rotation);
        auto newDegree = degree;
        if (ImGui::DragFloat3("rotation", &newDegree[0]))
        {
            auto delta = newDegree - degree;
            auto radians = glm::radians(delta);
            float length = glm::length(radians);
            if (length != 0)
                target->Rotate(length, glm::normalize(radians), RotationCoordinate::Self);
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
            ImGui::Text("%s", c.GetName().c_str());

            if (typeid(c) == typeid(Light))
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

                auto frameGraph = camera.GetFrameGraph();
                const char* buttonName = "empty";
                if (frameGraph)
                {
                    buttonName = frameGraph->GetName().c_str();
                }
                ImGui::Text("Frame Graph: ");
                ImGui::SameLine();
                if (ImGui::Button(buttonName))
                {
                    EditorState::selectedObject = frameGraph;
                }

                if (ImGui::BeginDragDropTarget())
                {
                    auto payload = ImGui::AcceptDragDropPayload("object");
                    if (payload && payload->IsDelivery())
                    {
                        if (FrameGraph::Graph* fg = dynamic_cast<FrameGraph::Graph*>(*(Object**)payload->Data))
                        {
                            camera.SetFrameGraph(fg);
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                ImGui::SameLine();
                if (ImGui::Button("Clear"))
                {
                    camera.SetFrameGraph(nullptr);
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
