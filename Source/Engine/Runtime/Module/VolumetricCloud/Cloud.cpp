#include "Cloud.hpp"
#include "Engine/Runtime/Object/GameObject/GameObject.hpp"
#include "Engine/Runtime/System/SceneManager/Scene.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Runtime/System/Rendering/Graphics.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"
#include <spdlog/spdlog.h>

DEFINE_OBJECT(Component, Cloud, "D659B514-6D77-498B-88DB-F20FC0F62B10");
Cloud::Cloud() : Component(nullptr) {};

Cloud::Cloud(GameObject* owner) : Component(owner) {};

void Cloud::OnLoaded() {}

void Cloud::Serialize(Serializer* s) const
{
    Component::Serialize(s);
    SERIALIZE(s, volumetricCloud);
    SERIALIZE(s, noiseGenerator);
    SERIALIZE(s, highFrequencyNoiseGenerator);
}

void Cloud::Deserialize(Serializer* s)
{
    Component::Deserialize(s);
    DESERIALIZE(s, volumetricCloud);
    DESERIALIZE(s, noiseGenerator);
    DESERIALIZE(s, highFrequencyNoiseGenerator);
}

std::unique_ptr<Component> Cloud::Clone(GameObject& owner)
{
    auto clone = std::make_unique<Cloud>(&owner);

    return clone;
}

const std::string& Cloud::GetName() const
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
        renderingScene->AddRenderObject(*this);
    }
}
void Cloud::RemoveFromRenderingScene()
{
    Scene* scene = GetScene();

    if (scene)
    {
        auto renderingScene = &scene->GetRenderingScene();
        renderingScene->RemoveRenderObject(*this);
    }
}

void Cloud::OnEnable()
{
    AddToRenderingScene();

    Setup();
}

void Cloud::OnDisable()
{
    RemoveFromRenderingScene();
}

void Cloud::UpdateNoiseTexture()
{
    auto cmd = GetGfxDriver()->CreateCommandBuffer();

    cmd->BindResource(noiseGenerator->GetSet(Gfx::DescriptorSetSemantics::Material), noiseGenerator->GetShaderResource());
    int dispatchX = glm::ceil(cloudNoise.baseShapeNoise->GetDescription().width / 8.0f);
    int dispatchY = glm::ceil(cloudNoise.baseShapeNoise->GetDescription().height / 8.0f);
    int dispatchZ = glm::ceil(cloudNoise.baseShapeNoise->GetDescription().depth / 8.0f);
    cmd->BindShaderProgram(noiseGenerator->GetShaderProgram(), noiseGenerator->GetPipelineConfig());
    cmd->Dispatch(dispatchX, dispatchY, dispatchZ);

    cmd->BindResource(
        highFrequencyNoiseGenerator->GetSet(Gfx::DescriptorSetSemantics::Material),
        highFrequencyNoiseGenerator->GetShaderResource()
    );
    dispatchX = glm::ceil(cloudNoise.highFrequencyNoise->GetDescription().width / 8.0f);
    dispatchY = glm::ceil(cloudNoise.highFrequencyNoise->GetDescription().height / 8.0f);
    dispatchZ = glm::ceil(cloudNoise.highFrequencyNoise->GetDescription().depth / 8.0f);
    cmd->BindShaderProgram(
        highFrequencyNoiseGenerator->GetShaderProgram(),
        highFrequencyNoiseGenerator->GetPipelineConfig()
    );
    cmd->Dispatch(dispatchX, dispatchY, dispatchZ);

    GetGfxDriver()->ExecuteCommandBuffer(*cmd);
}

Gfx::Image* Cloud::UpdateDebugImage(int debugImageIndex)
{
    if (debugImage == nullptr)
    {
        Gfx::ImageDescription desc(cloudSideResolution, cloudSideResolution, 1, Gfx::GfxFormat::R8G8B8A8_UNorm);
        debugImage = GetGfxDriver()->CreateImage(desc, Gfx::ImageUsage::Texture | Gfx::ImageUsage::ColorAttachment);
        debugImageMaterial = std::make_unique<Material>();
        debugImageMaterial->SetFlags(AssetStateFlags::DontSave);
        debugImageMaterial->SetShader(ShaderLibrary::GetShader("Specific/TextureDebug3D"));
        debugImageMaterial->SetFloat("layer", 0);
        debugImageMaterial->SetFloat("axis", 0);
    }
    debugImageMaterial->SetTexture(
        "tex",
        debugImageIndex == 0 ? cloudNoise.baseShapeNoise.get() : cloudNoise.highFrequencyNoise.get()
    );

    auto cmd = GetGfxDriver()->CreateCommandBuffer();
    debugRenderPass.SetAttachment(0, Gfx::ImageIdentifier(*debugImage));
    Gfx::ClearValue clears[] = {{0, 0, 0, 0}};
    cmd->BeginRenderPass(debugRenderPass, clears);
    cmd->BindResource(debugImageMaterial->GetSet(Gfx::DescriptorSetSemantics::Material), debugImageMaterial->GetShaderResource());
    cmd->BindShaderProgram(debugImageMaterial->GetShaderProgram(), debugImageMaterial->GetPipelineConfig());
    cmd->Draw(6, 1, 0, 0);
    cmd->EndRenderPass();

    GetGfxDriver()->ExecuteCommandBuffer(*cmd);

    return debugImage.get();
}

void Cloud::Tick()
{
    ObjPtr<Cloud> inSelf = this;
    Graphics::AddRenderingEvent(
        "cloud",
        RenderingEvent::Skybox,
        [inSelf](Gfx::CommandBuffer& cmd, const Rendering::RenderingData& renderingData)
        {
        }
    );
}

void Cloud::IdleTick()
{
    Tick();
}

void Cloud::ResetToDefaultValues()
{
    volumetricCloud->SetShader(ShaderLibrary::GetShader(volumetricCloudShader));
    volumetricCloud->SetVector("stratusType", {-0.36f, 0.17f, 0.57f, 0.62f});
    volumetricCloud->SetVector("areaMapRemap", {-0.22f, 1.0f, 0.0f, 0.0f});
    volumetricCloud->SetVector("ambientColor", {1.0f, 1.0f, 1.0f, 1.0f});
    volumetricCloud->SetVector("cloudCenter", {0.0f, 0.0f, 0.0f, 0.0f});
    volumetricCloud->SetVector("cloudExtent", {64.0f, 10.0f, 64.0f, 0.0f});
    volumetricCloud->SetFloat("fixedStep", 0.01f);
    volumetricCloud->SetFloat("extinction", 2.0f);
    volumetricCloud->SetFloat("lightExtinction", 0.1f);
    volumetricCloud->SetFloat("lightPhase_g", 0.3f);
    volumetricCloud->SetFloat("lightPhase_K", 0.8f);
    volumetricCloud->SetFloat("detailMapScale", 256.0f);
    volumetricCloud->SetFloat("highFrequencyMapScale", 6.0f);
    volumetricCloud->SetFloat("ambientScale", 0.05f);
    volumetricCloud->SetFloat("lightScale", 6.0f);
    volumetricCloud->SetFloat("lightStep", 0.5f);
    volumetricCloud->SetFloat("lightStepCount", 64.0f);
    volumetricCloud->SetFloat("globalScale", 1.0f);

    auto resetNoiseGenerator = [](Material& material)
    {
        material.SetFloat("perlin_Lacunarity", 2.0f);
        material.SetFloat("perlin_Gain", 0.5f);
        material.SetFloat("perlin_Amplitude", 0.5f);
        material.SetFloat("perlin_Frequency", 1.0f);
        material.SetFloat("octaves", 8.0f);
        material.SetFloat("worley_Lacunarity", 2.0f);
        material.SetFloat("worley_Gain", 0.5f);
        material.SetFloat("worley_Amplitude", 0.5f);
        material.SetFloat("worley_Frequency", 1.0f);
    };

    noiseGenerator->SetShader(ShaderLibrary::GetShader(cloudNoiseGeneratorShader));
    highFrequencyNoiseGenerator->SetShader(ShaderLibrary::GetShader(cloudNoiseGeneratorShader));
    resetNoiseGenerator(*noiseGenerator);
    resetNoiseGenerator(*highFrequencyNoiseGenerator);

    if (cloudNoise.baseShapeNoise && cloudNoise.highFrequencyNoise)
    {
        noiseGenerator->SetTexture("imgOutput", cloudNoise.baseShapeNoise.get());
        highFrequencyNoiseGenerator->SetTexture("imgOutput", cloudNoise.highFrequencyNoise.get());
        UpdateNoiseTexture();
    }
}

void Cloud::Setup()
{
    if (!isSetup)
    {
        isSetup = true;
        auto desc = Gfx::ImageDescription(
            cloudSideResolution,
            cloudSideResolution,
            cloudSideResolution,
            Gfx::GfxFormat::R8G8B8A8_UNorm
        );
        cloudNoise.baseShapeNoise =
            GetGfxDriver()->CreateImage(desc, Gfx::ImageUsage::Storage | Gfx::ImageUsage::Texture);

        desc.width = 32;
        desc.height = 32;
        desc.depth = 32;
        cloudNoise.highFrequencyNoise =
            GetGfxDriver()->CreateImage(desc, Gfx::ImageUsage::Storage | Gfx::ImageUsage::Texture);

        noiseGenerator->SetShader(ShaderLibrary::GetShader(cloudNoiseGeneratorShader));
        noiseGenerator->SetTexture("imgOutput", cloudNoise.baseShapeNoise.get());
        highFrequencyNoiseGenerator->SetShader(ShaderLibrary::GetShader(cloudNoiseGeneratorShader));
        highFrequencyNoiseGenerator->SetTexture("imgOutput", cloudNoise.highFrequencyNoise.get());

        UpdateNoiseTexture();
    }
}

void Cloud::TransformChanged() {}
