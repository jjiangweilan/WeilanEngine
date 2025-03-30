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
        if (ImGui::Button("Update Skybox Probe"))
        {
            target->UpdateSkyboxProbe();
        }

        ImGui::Checkbox("Debug Skybox Probe", &enableSkyboxProbeDebug);
        if (enableSkyboxProbeDebug)
        {
            target->DebugDrawSkyboxProbe(skyboxProbeDebugPosition);
            ImGui::InputFloat3("Skybox Probe Pos", &skyboxProbeDebugPosition[0]);
        }
    }

private:
    bool enableSkyboxProbeDebug = false;
    float3 skyboxProbeDebugPosition = {};
    static const char _register;
};

const char SceneSettingsInspector::_register = InspectorRegistry::Register<SceneSettingsInspector, SceneSettings>();
} // namespace Editor
