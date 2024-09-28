#include "../../EditorState.hpp"
#include "../Inspector.hpp"
#include "Core/Component/Camera.hpp"
#include "Core/Scene/Scene.hpp"
#include "EditorGUI/ObjectField.hpp"
#include "Rendering/FrameGraph/FrameGraph.hpp"

namespace Editor
{
class CameraInspector : public Inspector<Camera>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        Inspector<Camera>::DrawInspector(editor);

        ImGui::NewLine();
        ImGui::Text("Environment");
        ImGui::Separator();
        Texture* skybox = target->GetDiffuseEnv().Get();
        Texture* specularEnv = target->GetSpecularEnv().Get();
        if (GUI::ObjectField("Diffuse Environment Map", skybox))
        {
            if (skybox)
                target->SetDiffuseEnv(skybox);
        }

        if (GUI::ObjectField("Specular Environment Map", specularEnv))
        {
            if (specularEnv)
                target->SetSpecularEnv(specularEnv);
        }

        ImGui::NewLine();
        if (ImGui::Button("Set as main camera"))
        {
            if (EditorState::activeScene)
            {
                EditorState::activeScene->SetMainCamera(target);
            }
        }

        auto frameGraph = target->GetFrameGraph();
        const char* buttonName = "empty";
        if (frameGraph)
        {
            buttonName = frameGraph->GetName().c_str();
        }
        ImGui::Text("Frame Graph: ");
        ImGui::SameLine();
        if (ImGui::Button(buttonName))
        {
            EditorState::SelectObject(frameGraph->GetSRef());
        }

        if (ImGui::BeginDragDropTarget())
        {
            auto payload = ImGui::AcceptDragDropPayload("object");
            if (payload && payload->IsDelivery())
            {
                if (Rendering::FrameGraph::Graph* fg =
                        dynamic_cast<Rendering::FrameGraph::Graph*>(*(Object**)payload->Data))
                {
                    target->SetFrameGraph(fg);
                }
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::SameLine();
        if (ImGui::Button("Clear"))
        {
            target->SetFrameGraph(nullptr);
        }
    }

private:
    static const char _register;
};

const char CameraInspector::_register = InspectorRegistry::Register<CameraInspector, Camera>();

} // namespace Editor
