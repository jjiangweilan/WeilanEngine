#include "SceneEnvironment.hpp"
#include "Engine/Runtime/System/SceneManager/Scene.hpp"
#include "Engine/Runtime/Object/Texture/Texture.hpp"

DEFINE_OBJECT(Component, SceneEnvironment, "7E237F73-396E-455F-B227-657432A66877");

const std::string& SceneEnvironment::GetName() const
{
    static std::string name = "SceneEnvironment";
    return name;
}

void SceneEnvironment::OnEnable()
{
    auto scene = GetScene();

    if (scene)
    {
        auto& renderingScene = scene->GetRenderingScene();
        renderingScene.SetSceneEnvironment(*this);
    }
}
void SceneEnvironment::OnDisable()
{
    auto scene = GetScene();

    if (scene)
    {
        auto& renderingScene = scene->GetRenderingScene();
        renderingScene.RemoveSceneEnvironment(*this);
    }
}

void SceneEnvironment::Serialize(Serializer* s) const
{
    Component::Serialize(s);
}

void SceneEnvironment::Deserialize(Serializer* s)
{
    Component::Deserialize(s);
}

void SceneEnvironment::UpdateSkyboxProbe()
{
    auto scene = GetScene();
    if (scene == nullptr)
        return;

    SHProbeUpdateSettings shProbeUpdateSettings;

    shProbeUpdateSettings.skyboxOnly = true;
    skyboxProbe.UpdateProbe(*scene, shProbeUpdateSettings);
}

void SceneEnvironment::DebugDrawSkyboxProbe(const float3& position)
{
    skyboxProbe.DebugDrawProbe(position);
}
