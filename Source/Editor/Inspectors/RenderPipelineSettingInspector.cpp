#include "Editor/EditorState.hpp"
#include "Inspector.hpp"
#include "Library/Serialization/JsonSerializer.hpp"
#include "Runtime/System/Rendering/RenderPipeline/RenderPipelineSetting.hpp"

using namespace Rendering;
namespace Editor
{
class RenderPipelineSettingInspector : public Inspector<RenderPipelineSetting>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        Inspector<RenderPipelineSetting>::DrawInspector(editor);
        EditorGUI::AutoObjectInspector(target);
    }

private:
    static const char _register;
};

const char RenderPipelineSettingInspector::_register =
    InspectorRegistry::Register<RenderPipelineSettingInspector, RenderPipelineSetting>();

} // namespace Editor
