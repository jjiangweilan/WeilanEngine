#include "../../EditorState.hpp"
#include "../Inspector.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Component/Camera.hpp"
#include "Core/Scene/Scene.hpp"
#include "DragDropIDs.hpp"
#include "EditorGUI.hpp"
#include "Rendering/FrameGraph/FrameGraph.hpp"
#include "Rendering/Graphics.hpp"
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

        Object* frameGraphPayload;
        if (GUI::DragDropTarget(typeid(Rendering::FrameGraph::Graph), frameGraphPayload))
        {
            target->SetFrameGraph((Rendering::FrameGraph::Graph*)frameGraphPayload);
        }

        ImGui::SameLine();
        if (ImGui::Button("Clear"))
        {
            target->SetFrameGraph(nullptr);
        }

        Graphics::DrawFrustum(target->GetProjectionMatrix() * target->GetViewMatrix());
    }

private:
    static const char _register;
};

const char CameraInspector::_register = InspectorRegistry::Register<CameraInspector, Camera>();

} // namespace Editor
