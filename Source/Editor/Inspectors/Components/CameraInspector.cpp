#include "../../EditorState.hpp"
#include "../Inspector.hpp"
#include "Core/Component/Camera.hpp"
#include "Core/Scene/Scene.hpp"
#include "Rendering/FrameGraph/FrameGraph.hpp"

namespace Editor
{
class CameraInspector : public Inspector<Camera>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        Inspector<Camera>::DrawInspector(editor);

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
            EditorState::selectedObject = frameGraph->GetSRef();
        }

        if (ImGui::BeginDragDropTarget())
        {
            auto payload = ImGui::AcceptDragDropPayload("object");
            if (payload && payload->IsDelivery())
            {
                if (FrameGraph::Graph* fg = dynamic_cast<FrameGraph::Graph*>(*(Object**)payload->Data))
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
