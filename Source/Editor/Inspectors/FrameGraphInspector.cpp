#include "../EditorState.hpp"
#include "FrameGraph/FrameGraphEditor.hpp"
#include "Inspector.hpp"
#include "Rendering/FrameGraph/FrameGraph.hpp"

namespace Engine::Editor
{
class FrameGraphInspector : public Inspector<FrameGraph::Graph>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        fgEditor.Draw(target->GetEditorContext(), *target);
    }

private:
    FrameGraphEditor fgEditor;
    static const char _register;
};

const char FrameGraphInspector::_register = InspectorRegistry::Register<FrameGraphInspector, FrameGraph::Graph>();

} // namespace Engine::Editor
