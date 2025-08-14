#include "Core/Scene/SceneSettings.hpp"
#include "Inspector.hpp"
#include "InspectorHelper_Image.hpp"

namespace Editor
{
class SceneSettingsInspector : public Inspector<SceneSettings>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        if (GUI::ButtonSimple("Update Skybox Probe"))
        {
            target->UpdateSkyboxProbe();
        }

        GUI::Checkbox("Debug Skybox Probe", &enableSkyboxProbeDebug);
        if (enableSkyboxProbeDebug)
        {
            target->DebugDrawSkyboxProbe(skyboxProbeDebugPosition);
            GUI::DragFloat3("Skybox Probe Position", &skyboxProbeDebugPosition[0]);
        }
    }

private:
    bool enableSkyboxProbeDebug = false;
    float3 skyboxProbeDebugPosition = {};
    static const char _register;
};

const char SceneSettingsInspector::_register = InspectorRegistry::Register<SceneSettingsInspector, SceneSettings>();
} // namespace Editor
