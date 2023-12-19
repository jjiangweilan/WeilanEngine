#include "../EditorState.hpp"
#include "FrameGraph/FrameGraphEditor.hpp"
#include "Inspector.hpp"
#include "Rendering/FrameGraph/FrameGraph.hpp"

namespace Editor
{
class FrameGraphInspector : public Inspector<FrameGraph::Graph>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        char name[256];
        auto targetName = target->GetName();
        strcpy(name, targetName.c_str());
        if (ImGui::InputText("name", name, 256))
        {
            target->SetName(name);
            target->SetDirty();
        }
        fgEditor.Draw(target->GetEditorContext(), *target);
    }

private:
    FrameGraphEditor fgEditor;
    static const char _register;
};

const char FrameGraphInspector::_register = InspectorRegistry::Register<FrameGraphInspector, FrameGraph::Graph>();

} // namespace Editor
