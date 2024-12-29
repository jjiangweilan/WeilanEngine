#include "../EditorState.hpp"
#include "Inspector.hpp"
#include "Libs/Serialization/JsonSerializer.hpp"
#include "Rendering/RenderPipeline/RenderPipelineSetting.hpp"

using namespace Rendering;
namespace Editor
{
class RenderPipelineSettingInspector : public Inspector<RenderPipelineSetting>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        Inspector<RenderPipelineSetting>::DrawInspector(editor);
        GUI::AutoObjectInspector(target);
    }

private:
    static const char _register;
};

const char RenderPipelineSettingInspector::_register =
    InspectorRegistry::Register<RenderPipelineSettingInspector, RenderPipelineSetting>();

} // namespace Editor
