#include "SceneSettings.hpp"

DEFINE_OBJECT(Component, SceneSettings, "1F6B289E-09E9-4AC4-951D-B5DD1FDE8E6B")

const std::string& SceneSettings::GetName() const
{
    static std::string name = "SceneSettings";
    return name;
}

void SceneSettings::UpdateSkyboxProbe()
{
    auto scene = GetScene();
    if (scene == nullptr)
        return;

    SHProbeUpdateSettings shProbeUpdateSettings;

    shProbeUpdateSettings.skyboxOnly = true;
    skyboxProbe.UpdateProbe(*scene, shProbeUpdateSettings);
}

void SceneSettings::DebugDrawSkyboxProbe(const float3& position)
{
    skyboxProbe.DebugDrawProbe(position);
}
