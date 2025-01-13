#include "Cloud.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "Core/EngineInternalResources.hpp"
#include "Core/GameObject.hpp"
#include "Core/Scene/Scene.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Rendering/RenderPipeline/Renderers/CloudRenderer.hpp"
#include <spdlog/spdlog.h>

DEFINE_OBJECT(Cloud, "D659B514-6D77-498B-88DB-F20FC0F62B10");
Cloud::Cloud() : Component(nullptr) {};

Cloud::Cloud(GameObject* owner) : Component(owner)
{
    Setup();
};

void Cloud::OnLoaded()
{
    Setup();
}

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
    // this->volumetricCloud = AssetDatabase::Singleton()->SaveAsset(std::move(volumetricCloud), "volumetricCloud");
    // this->noiseGenerator = AssetDatabase::Singleton()->SaveAsset(std::move(noiseGenerator), "noiseGenerator");
}

void Cloud::OnEnable()
{
    AddToRenderingScene();
}

void Cloud::OnDisable()
{
    RemoveFromRenderingScene();
}

void Cloud::UpdateNoiseTexture()
{
    auto cmd = GetGfxDriver()->CreateCommandBuffer();

    cmd->BindResource(noiseGenerator->GetSet("perMaterial"), noiseGenerator->GetShaderResource());
    int dispatchX = glm::ceil(cloudNoise.desc.width / 8.0f);
    int dispatchY = glm::ceil(cloudNoise.desc.height / 8.0f);
    int dispatchZ = glm::ceil(cloudNoise.desc.depth / 8.0f);
    cmd->BindShaderProgram(noiseGenerator->GetShaderProgram(), noiseGenerator->GetShaderConfig());
    cmd->Dispatch(dispatchX, dispatchY, dispatchZ);

    GetGfxDriver()->ExecuteCommandBuffer(*cmd);
}

void Cloud::Setup()
{
    if (!isSetup)
    {
        isSetup = true;
        cloudNoise.desc = Gfx::ImageDescription(512, 512, 64, Gfx::GfxFormat::R8G8B8A8_UNorm);
        cloudNoise.tex =
            GetGfxDriver()->CreateImage(cloudNoise.desc, Gfx::ImageUsage::Storage | Gfx::ImageUsage::Texture);

        volumetricCloud->SetShader(ShaderLibrary::GetShader(volumetricCloudShader));
        noiseGenerator->SetShader(ShaderLibrary::GetShader(cloudNoiseGeneratorShader));
        noiseGenerator->SetTexture("imgOutput", cloudNoise.tex.get());

        auto meshRenderer = GetGameObject()->GetComponent<MeshRenderer>();
        if (meshRenderer == nullptr)
            meshRenderer = GetGameObject()->AddComponent<MeshRenderer>();

        meshRenderer->SetMesh(EngineInternalResources::GetModels().cube);
        meshRenderer->SetMaterial(volumetricCloud.get());
        volumetricCloud->SetTexture("cloudDensity", cloudNoise.tex.get());
        UpdateNoiseTexture();
        UpdateCloudGPUProperties();
    }
}

void Cloud::TransformChanged()
{
    UpdateCloudGPUProperties();
}

void Cloud::UpdateCloudGPUProperties()
{
    auto go = GetGameObject();
    const glm::float3 scale = go->GetScale();
    volumetricCloud->SetVector("cubePos", glm::float4(go->GetPosition() - scale / 2.0f, 1.0));
    volumetricCloud->SetVector("cubeExtent", glm::float4(scale, 1.0));
}
