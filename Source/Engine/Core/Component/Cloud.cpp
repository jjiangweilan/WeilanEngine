#include "Cloud.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/GameObject.hpp"
#include "Core/Scene/Scene.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Rendering/RenderPipeline/Renderers/CloudRenderer.hpp"
#include <spdlog/spdlog.h>

DEFINE_OBJECT(Cloud, "D659B514-6D77-498B-88DB-F20FC0F62B10");
Cloud::Cloud() : Component(nullptr) {};

Cloud::Cloud(GameObject* owner) : Component(owner) {};

void Cloud::Serialize(Serializer* s) const
{
    Component::Serialize(s);
    SERIALIZE(s, volumetricCloud);
    SERIALIZE(s, noiseGenerator);
}

void Cloud::Deserialize(Serializer* s)
{
    Component::Deserialize(s);
    DESERIALIZE(s, volumetricCloud);
    DESERIALIZE(s, noiseGenerator);
}

std::unique_ptr<Component> Cloud::Clone(GameObject& owner)
{
    auto clone = std::make_unique<Cloud>(&owner);

    return clone;
}

const std::string& Cloud::GetName()
{
    static std::string name = "Cloud";
    return name;
}

void Cloud::AddToRenderingScene()
{
    Scene* scene = GetScene();

    if (scene)
    {
        auto renderingScene = &scene->GetRenderingScene();
        renderingScene->AddRenderer(*this);
    }
}
void Cloud::RemoveFromRenderingScene()
{
    Scene* scene = GetScene();

    if (scene)
    {
        auto renderingScene = &scene->GetRenderingScene();
        renderingScene->RemoveRenderer(*this);
    }
}

void Cloud::CreateCloudMaterials()
{
    std::unique_ptr<Material> volumetricCloud;
    std::unique_ptr<Material> noiseGenerator;

    Rendering::CloudRenderer::CreateCloudMaterials(volumetricCloud, noiseGenerator);
    this->volumetricCloud = AssetDatabase::Singleton()->SaveAsset(std::move(volumetricCloud), "volumetricCloud");
    this->noiseGenerator = AssetDatabase::Singleton()->SaveAsset(std::move(noiseGenerator), "noiseGenerator");
}

void Cloud::OnEnable()
{
    AddToRenderingScene();
}

void Cloud::OnDisable()
{
    RemoveFromRenderingScene();
}
