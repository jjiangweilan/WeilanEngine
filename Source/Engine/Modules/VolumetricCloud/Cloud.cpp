#include "Cloud.hpp"
#include "Core/GameObject.hpp"
#include "Core/Scene/Scene.hpp"
#include "GfxDriver/GfxDriver.hpp"
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

Gfx::Image* Cloud::UpdateDebugImage()
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
        debugImageMaterial->SetTexture("tex", cloudNoise.tex.get());
    }

    auto cmd = GetGfxDriver()->CreateCommandBuffer();
    debugRenderPass.SetAttachment(0, Gfx::RG::ImageIdentifier(*debugImage));
    Gfx::ClearValue clears[] = {{0, 0, 0, 0}};
    cmd->BeginRenderPass(debugRenderPass, clears);
    cmd->BindResource(debugImageMaterial->GetSet("perMaterial"), debugImageMaterial->GetShaderResource());
    cmd->BindShaderProgram(debugImageMaterial->GetShaderProgram(), debugImageMaterial->GetShaderConfig());
    cmd->Draw(6, 1, 0, 0);
    cmd->EndRenderPass();

    GetGfxDriver()->ExecuteCommandBuffer(*cmd);

    return debugImage.get();
}

void Cloud::Tick()
{
    ObjPtr<Cloud> self = this;
    GetScene()->GetRenderingScene().Draw(
        "cloud",
        RenderingEvent::Skybox,
        [self](Gfx::CommandBuffer& cmd, const Rendering::RenderingData& renderingData)
        {
            if (self == nullptr)
                return;

            auto scene = self->GetScene();
            self->volumetricCloud->SetTexture("mainColor", renderingData.mainColor);
            self->volumetricCloud->SetTexture("interleavedGradientNoise", renderingData.interleavedGradientNoise.get());
            self->volumetricCloud->SetTexture(
                "depthMap",
                renderingData.mainDepth,
                Gfx::ImageViewOption{0, 1, 0, 1, Gfx::ImageAspect::Depth}
            );
            cmd.BindShaderProgram(
                self->volumetricCloud->GetShader()->GetShaderProgram(),
                self->volumetricCloud->GetShaderConfig()
            );
            cmd.BindResource(self->volumetricCloud->GetSet("perMaterial"), self->volumetricCloud->GetShaderResource());
            cmd.Dispatch(
                (renderingData.sceneInfo->screenSize.x + 7) / 8,
                (renderingData.sceneInfo->screenSize.y + 7) / 8,
                1
            );
        }
    );
}

void Cloud::IdleTick()
{
    Tick();
}

void Cloud::Setup()
{
    if (!isSetup)
    {
        isSetup = true;
        cloudNoise.desc = Gfx::ImageDescription(
            cloudSideResolution,
            cloudSideResolution,
            cloudHeightResolution,
            Gfx::GfxFormat::R8G8B8A8_UNorm
        );
        cloudNoise.tex =
            GetGfxDriver()->CreateImage(cloudNoise.desc, Gfx::ImageUsage::Storage | Gfx::ImageUsage::Texture);

        volumetricCloud->SetShader(ShaderLibrary::GetShader(volumetricCloudShader));
        noiseGenerator->SetShader(ShaderLibrary::GetShader(cloudNoiseGeneratorShader));
        noiseGenerator->SetTexture("imgOutput", cloudNoise.tex.get());

        volumetricCloud->SetTexture("cloudDensity", cloudNoise.tex.get());
        UpdateNoiseTexture();
    }
}

void Cloud::TransformChanged()
{
}
