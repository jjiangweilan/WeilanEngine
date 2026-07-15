#include "Editor/EditorState.hpp"
#include "Editor/Inspectors/Inspector.hpp"
#include "Engine/Library/Serialization/JsonSerializer.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipelineSetting.hpp"
#include "Engine/ThirdParty/imgui/imgui.h"

using namespace Rendering;
namespace Editor
{
class RenderPipelineSettingInspector : public Inspector<RenderPipelineSetting>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        Inspector<RenderPipelineSetting>::DrawInspector(editor);

        int antiAliasing = static_cast<int>(target->antiAliasing);
        const char* antiAliasingModes[] = {"None", "FXAA", "TAA"};
        if (EditorGUI::ComboLabeled("Anti Aliasing", &antiAliasing, antiAliasingModes, IM_ARRAYSIZE(antiAliasingModes)))
        {
            target->antiAliasing = static_cast<RenderPipelineSetting::AntiAliasingMode>(antiAliasing);
            target->SetDirty();
        }

        EditorGUI::AutoObjectInspector(target, false, {"antiAliasing"});
    }

private:
    static const char _register;
};

const char RenderPipelineSettingInspector::_register =
    InspectorRegistry::Register<RenderPipelineSettingInspector, RenderPipelineSetting>();

} // namespace Editor
