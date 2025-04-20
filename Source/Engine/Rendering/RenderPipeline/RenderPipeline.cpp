#include "RenderPipeline.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Component/ParticleSystem.hpp"
#include "Core/Component/SceneEnvironment.hpp"
#include "Core/Scene/Scene.hpp"
#include "Core/Texture.hpp"
#include "Core/Time.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Profiler/Profiler.hpp"
#include "Rendering/Graphics.hpp"
#include "Rendering/Renderers/ParticleRenderer.hpp"
#include "Rendering/RenderingUtils.hpp"
#include "Rendering/ShaderLibrary.hpp"

namespace Rendering
{

RenderPipeline::RenderPipeline()
{
    particleRenderer = std::make_unique<ParticleRenderer>();
    shadowRenderer = std::make_unique<ShadowRenderer>();
    shadowRenderer->Init();

    commandBuffer = GetGfxDriver()->CreateCommandBuffer();

    Gfx::RG::SubpassAttachment skyboxOnlyPassAttachment[] = {
        {0, Gfx::AttachmentLoadOperation::Clear, Gfx::AttachmentStoreOperation::Store}
    };
    skyboxOnlyPass.SetSubpass(0, skyboxOnlyPassAttachment);
}

RenderPipeline::~RenderPipeline() {}

void RenderPipeline::Render(Scene& scene, Camera& camera, glm::float2 screenSize)
{
    ENGINE_BEGIN_PROFILE("RenderPipeline - Setup")
    setting = scene.GetRenderPipelineSetting();
    Gfx::CommandBuffer* cmd = GetCommandBuffer();

    cmd->BeginLabel("Render Scene", {0.623, 0.323, 0.4123, 1.0f});
    if (!FrameSetup(cmd, scene, camera, screenSize))
    {
        return;
    }

    // Setup
    glm::float2 mainRTSize = {mainColorDescription.GetWidth(), mainColorDescription.GetHeight()};

    DrawList sceneDrawList;
    SceneRendererSorter()(scene, camera, sceneDrawList);
    ENGINE_END_PROFILE

    cmd->BindResource(0, perScene.gpuResourceSet.get());

    // Shadow Pass
    shadowRenderer->Execute(*cmd, sceneDrawList);

    // GBuffer Pass
    cmd->BeginLabel("GBuffer", &labelColors.passColor[0]);
    {
        albedoGBufferDescription.SetWidth(mainRTSize.x);
        albedoGBufferDescription.SetHeight(mainRTSize.y);
        albedoGBufferDescription.SetFormat(Gfx::GfxFormat::R8G8B8A8_SRGB);
        normalGBufferDescription.SetWidth(mainRTSize.x);
        normalGBufferDescription.SetHeight(mainRTSize.y);
        normalGBufferDescription.SetFormat(Gfx::GfxFormat::A2B10G10R10_UNorm);
        maskGBufferDescription.SetWidth(mainRTSize.x);
        maskGBufferDescription.SetHeight(mainRTSize.y);
        maskGBufferDescription.SetFormat(Gfx::GfxFormat::R8G8B8A8_UNorm);

        cmd->AllocateAttachment(albedoGBuffer, albedoGBufferDescription);
        cmd->AllocateAttachment(normalGBuffer, normalGBufferDescription);
        cmd->AllocateAttachment(maskGBuffer, maskGBufferDescription);

        // clear albedo to black
        // masks to 1 (mainly for ao)
        Gfx::ClearValue clears[] = {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {1.0f, 1.0f, 1.0f, 1.0f}, {1, 0}};
        gbufferPass.pass.SetAttachment(0, mainColor);
        gbufferPass.pass.SetAttachment(1, albedoGBuffer);
        gbufferPass.pass.SetAttachment(2, normalGBuffer);
        gbufferPass.pass.SetAttachment(3, maskGBuffer);
        gbufferPass.pass.SetAttachment(4, mainDepth);
        cmd->BeginRenderPass(gbufferPass.pass, clears);

        // draw
        sceneDrawList.DrawRangeHelper(*cmd, 0, sceneDrawList.alphaTestIndex);
        sceneDrawList.DrawRangeHelper(*cmd, sceneDrawList.alphaTestIndex, sceneDrawList.transparentIndex);

        cmd->EndRenderPass();
    }
    cmd->EndLabel();

    // ambient occlusion pass
    ambientOcclusionPass.Execute(cmd, renderingData.mainDepth, setting, renderingData);

    // Shading
    cmd->BeginLabel("Shading", &labelColors.passColor[0]);
    {
        // Upload GPU Parameter
        {
            shadingPass.cpuParameter = GPUParameter::DeferredPBRShadingInput{
                .shadowMapTexelSize = shadowRenderer->GetShadowMapTexelSize(),
                .shadowConstantBias = setting->shadowMap.constantBias / 1000.0f,
                .shadowNormalBias = setting->shadowMap.normalBias
            };

            GetGfxDriver()->UploadBuffer(
                *shadingPass.perMaterialBuffer,
                (uint8_t*)&shadingPass.cpuParameter,
                sizeof(GPUParameter::DeferredPBRShadingInput)
            );
        }

        cmd->Blit(mainDepth, depthCopy);

        Gfx::ClearValue lightingPassClearValues[] = {{0, 0, 0, 0}, {0, 0}};
        auto shadingShader = shadingPass.shadingShader->GetShaderProgram();
        auto diffuseCube = camera.GetDiffuseEnv();
        auto specularCube = camera.GetDiffuseEnv();

        auto depthImage = GetGfxDriver()->GetImageFromRenderGraph(depthCopy);
        auto& depthImageView = depthImage->GetImageView({Gfx::ImageAspect::Depth});

        shadingPass.gpuResource->SetImage("albedoTex"_shaderBinding, albedoGBuffer);
        shadingPass.gpuResource->SetImage("normalTex"_shaderBinding, normalGBuffer);
        shadingPass.gpuResource->SetImage("maskTex"_shaderBinding, maskGBuffer);
        shadingPass.gpuResource->SetImage("depthTex"_shaderBinding, &depthImageView);
        shadingPass.gpuResource->SetImage("shadowMap"_shaderBinding, shadowRenderer->GetShadowMap());
        shadingPass.gpuResource->SetImage("ambientOcclusion"_shaderBinding, ambientOcclusionPass.ssao);
        if (diffuseCube)
            shadingPass.gpuResource->SetImage("diffuseCube"_shaderBinding, diffuseCube->GetGfxImage());
        if (specularCube)
            shadingPass.gpuResource->SetImage("specularCube"_shaderBinding, specularCube->GetGfxImage());

        shadingPass.pass.SetAttachment(0, mainColor);
        shadingPass.pass.SetAttachment(1, mainDepth);

        cmd->BeginRenderPass(shadingPass.pass, lightingPassClearValues);
        cmd->BindResource(1, shadingPass.gpuResource.get());
        cmd->BindShaderProgram(shadingShader, shadingShader->GetDefaultShaderConfig());
        cmd->Draw(6, 1, 0, 0);

        cmd->EndRenderPass();
    }
    cmd->EndLabel();

    // TODO: copy mainColor and mainDepth for special effects

    // Forward Pass
    cmd->BeginLabel("Forward", &labelColors.passColor[0]);
    {
        forwardPass.pass.SetAttachment(0, mainColor);
        forwardPass.pass.SetAttachment(1, mainDepth);

        Gfx::ClearValue clears[] = {{0, 0, 0, 0}, {0, 0}};
        cmd->BeginRenderPass(forwardPass.pass, clears);

        if (renderConfig.drawGraphics)
        {
            cmd->BeginLabel("Draw Graphics", &labelColors.passColor[0]);
            RenderingUtils::DrawGraphics(*cmd);
            cmd->EndLabel();
        }

        skyboxPass.Execute(cmd);

        cmd->EndRenderPass();

        // Graphics::GetSingleton().ExecuteRenderingEvnet(RenderingEvent::Skybox, *cmd, renderingData);
        auto clouds = scene.GetRenderingScene().GetClouds();
        if (!clouds.empty())
        {
            cloudPass.Execute(*clouds[0], *cmd, renderingData);
        }

        // draw objects
        cmd->BeginLabel("Forward Objects", {0.12, 0.64, 0.342, 1.0f});
        cmd->BeginRenderPass(forwardPass.pass, clears);
        sceneDrawList.DrawRangeHelper(*cmd, sceneDrawList.transparentIndex, sceneDrawList.size());
        cmd->EndLabel();

        // draw particles
        cmd->BindResource(0, GetPerSceneGPUResource());
        cmd->BeginLabel("Particles", {0.55, 0.11, 0.57, 1.0f});
        auto particleSystems = scene.GetRenderingScene().GetParticleSystems();
        for (auto p : particleSystems)
        {
            particleRenderer->Draw(*cmd, p->GetDraw());
        }
        cmd->EndLabel();

        cmd->EndRenderPass();
    }
    cmd->EndLabel();

    // start post procesing
    finalColor = mainColor;

    if (setting->postProcess.colorGrading)
    {
        cmd->BeginLabel("Color Grading", &labelColors.passColor[0]);
        {
            // TODO
            Gfx::RG::ImageDescription resultDesc(mainRTSize.x, mainRTSize.y, Gfx::GfxFormat::R8G8B8A8_SRGB);
            cmd->AllocateAttachment(colorGradingPass.colorGradingId, resultDesc);
            colorGradingPass.pass.SetAttachment(0, colorGradingPass.colorGradingId);
            colorGradingPass.mat.SetTexture("mainColor", renderingData.mainColor);
            Gfx::ClearValue clears[] = {{0, 0, 0, 0}};
            cmd->BeginRenderPass(colorGradingPass.pass, clears);
            cmd->BindShaderProgram(colorGradingPass.mat.GetShaderProgram(), colorGradingPass.mat.GetShaderConfig());
            cmd->BindResource(0, colorGradingPass.mat.GetShaderResource());
            cmd->Draw(6, 1, 0, 0);
            cmd->EndRenderPass();
            finalColor = colorGradingPass.colorGradingId;
        }
        cmd->EndLabel();
    }

    // FXAA
    if (setting->fxaa)
    {
        cmd->BeginLabel("FXAA", &labelColors.passColor[0]);
        {
            Gfx::RG::ImageDescription resultDesc(mainRTSize.x, mainRTSize.y, Gfx::GfxFormat::R8G8B8A8_SRGB);
            cmd->AllocateAttachment(fxaaPass.fxaaId, resultDesc);
            fxaaPass.Execute(*cmd, {mainRTSize.x, mainRTSize.y, 0, 0}, finalColor, fxaaPass.fxaaId);
            finalColor = fxaaPass.fxaaId;
        }
        cmd->EndLabel();
    }

    cmd->EndLabel(); // Render Scene

    if (!IsCommandBufferOverriden())
    {
        GetGfxDriver()->ExecuteCommandBuffer(*cmd);
        cmd->Reset(true);
    }
}

RenderPipeline::PerScene::PerScene()
{
    gpuBuffer = GetGfxDriver()
                    ->CreateBuffer(sizeof(GPUParameter::PerScene), Gfx::BufferUsage::Uniform, false, false, "PerScene");
    gpuResourceSet = GetGfxDriver()->CreateShaderResource();
    gpuResourceSet->SetBuffer("perScene", gpuBuffer.get());

    ASSERT(gpuBuffer->GetSize() == sizeof(GPUParameter::PerScene));
}

RenderPipeline::ShadingPass::ShadingPass()
{
    pass = Gfx::RG::RenderPass("shading", 1, 2);
    Gfx::RG::SubpassAttachment lightingPassAttachment{
        0,
        Gfx::AttachmentLoadOperation::Load,
        Gfx::AttachmentStoreOperation::Store
    };

    Gfx::RG::SubpassAttachment depthAttachment{
        1,
        Gfx::AttachmentLoadOperation::Load,
        Gfx::AttachmentStoreOperation::Store
    };
    Gfx::RG::SubpassAttachment lightingPassAttachments[] = {lightingPassAttachment};
    pass.SetSubpass(0, lightingPassAttachments, depthAttachment);
    gpuResource = GetGfxDriver()->CreateShaderResource();
    perMaterialBuffer = GetGfxDriver()->CreateBuffer(
        sizeof(GPUParameter::DeferredPBRShadingInput),
        Gfx::BufferUsage::Uniform,
        false,
        false,
        "Deferred PBR Shading Input"
    );
    gpuResource->SetBuffer("perMaterial", perMaterialBuffer.get());
    brdfPreIntegeral = (Texture*)AssetDatabase::Singleton()->LoadAsset("_engine_internal/Textures/BRDFPreintegral.ktx");
    gpuResource->SetImage("specularBRDFIntegrationMap", brdfPreIntegeral->GetGfxImage());
    shadingShader = ShaderLibrary::GetShader(ShaderLibrary::DeferredPBRShading);
}

RenderPipeline::GBufferPass::GBufferPass()
{
    pass = Gfx::RG::RenderPass("gbuffer", 1, 5);
    Gfx::RG::SubpassAttachment lighting{0, Gfx::AttachmentLoadOperation::Clear, Gfx::AttachmentStoreOperation::Store};
    Gfx::RG::SubpassAttachment albedo{1};
    Gfx::RG::SubpassAttachment normal{2};
    Gfx::RG::SubpassAttachment property{3};
    Gfx::RG::SubpassAttachment depth{4};
    Gfx::RG::SubpassAttachment subpassAttachments[] = {lighting, albedo, normal, property};
    pass.SetSubpass(0, subpassAttachments, depth);
}

RenderPipeline::ScreenSpaceShadow::ScreenSpaceShadow()
{
    shader = ShaderLibrary::GetShader(ShaderLibrary::ScreenSpaceShadow);
}

RenderPipeline::FXAAPass::FXAAPass()
{
    shader = ShaderLibrary::GetShader(ShaderLibrary::FXAA);
    resource = GetGfxDriver()->CreateShaderResource();

    Gfx::RG::SubpassAttachment attachmentDesc{
        0,
        Gfx::AttachmentLoadOperation::Load,
        Gfx::AttachmentStoreOperation::Store
    };
    Gfx::RG::SubpassAttachment attachments[] = {attachmentDesc};
    pass.SetSubpass(0, attachments);
}

RenderPipeline::ForwardPass::ForwardPass()
{
    pass = Gfx::RG::RenderPass::Default(
        "Forward Pass",
        Gfx::AttachmentLoadOperation::Load,
        Gfx::AttachmentStoreOperation::Store,
        Gfx::AttachmentLoadOperation::Load,
        Gfx::AttachmentStoreOperation::Store
    );
}

void RenderPipeline::FXAAPass::Execute(
    Gfx::CommandBuffer& cmd,
    const glm::float4& sourceSize,
    const Gfx::RG::ImageIdentifier& src,
    const Gfx::RG::ImageIdentifier& dst
)
{
    ;
    pass.SetAttachment(0, dst);
    resource->SetImage("source", GetGfxDriver()->GetImageFromRenderGraph(src));
    Gfx::ClearValue clears[] = {{0, 0, 0, 0}};
    cmd.BeginRenderPass(pass, clears);
    cmd.SetPushConstant(shader->GetShaderProgram(), (void*)&sourceSize[0]);
    cmd.BindResource(0, resource.get());
    cmd.BindShaderProgram(shader->GetShaderProgram(), shader->GetShaderProgram()->GetDefaultShaderConfig());
    cmd.Draw(6, 1, 0, 0);
    cmd.EndRenderPass();
}

RenderPipeline::ColorGradingPass::ColorGradingPass()
{
    colorGradingShader = ShaderLibrary::GetShader(ShaderLibrary::ColorGrading);
    mat.SetShader(colorGradingShader);
}

RenderPipeline::AmbientOcclusionPass::AmbientOcclusionPass()
{
    ssaoShader = ShaderLibrary::GetShader(ShaderLibrary::PostProcess_SSAO);
    mat.SetShader(ssaoShader);
}

void RenderPipeline::AmbientOcclusionPass::Execute(
    Gfx::CommandBuffer* cmd, Gfx::Image* texDepth, RenderPipelineSetting* setting, RenderingData& renderingData
)
{
    mat.SetFloat("strength", setting->ssao.strength);
    mat.SetFloat("scaling", setting->ssao.scaling);
    mat.SetFloat("falloff", setting->ssao.falloff);
    mat.SetFloat("bias", setting->ssao.bias);

    mat.SetVector("rtSize", renderingData.sceneInfo->screenSize);
    mat.SetTexture("depthTex", texDepth);

    Gfx::ClearValue clears[] = {{0, 0, 0, 0}};
    Gfx::RG::ImageDescription desc(
        renderingData.sceneInfo->screenSize.x,
        renderingData.sceneInfo->screenSize.y,
        Gfx::GfxFormat::R32_SFloat
    );

    cmd->AllocateAttachment(ssao, desc);

    pass.SetAttachment(0, ssao);
    cmd->BeginLabel("SSAO", {0.3, 0.1, 0.5, 1.0});
    cmd->BeginRenderPass(pass, clears);
    cmd->BindResource(mat.GetSet(Gfx::DescriptorSetSemantics::Material), mat.GetShaderResource());
    cmd->BindShaderProgram(ssaoShader->GetShaderProgram(), ssaoShader->GetShaderProgram()->GetDefaultShaderConfig());
    cmd->Draw(6, 1, 0, 0);
    cmd->EndRenderPass();
    cmd->EndLabel();
}

void SceneRendererSorter::operator()(Scene& scene, Camera& camera, Rendering::DrawList& outDrawList)
{
    outDrawList.clear();
    outDrawList.Add(scene.GetRenderingScene().GetMeshRenderers());
    outDrawList.Sort(camera.GetGameObject()->GetPosition());
}

void RenderPipeline::RenderSkyboxOnly(Scene& scene, Camera& camera, glm::float2 screenSize)
{
    auto cmd = GetCommandBuffer();

    setting = scene.GetRenderPipelineSetting();

    if (renderConfig.colorOutputOverride.has_value())
    {
        screenSize = renderConfig.colorOutputOverride.value()->GetImage().GetDescription().GetSize();
    }

    if (!FrameSetup(cmd, scene, camera, screenSize))
        return;

    cmd->BindResource(0, perScene.gpuResourceSet.get());

    Gfx::ClearValue clears[] = {{0, 0, 0, 0}};
    auto finalColor = GetFinalColor();
    skyboxOnlyPass.SetAttachment(0, finalColor);
    cmd->BeginRenderPass(skyboxOnlyPass, clears);
    skyboxPass.Execute(cmd);
    cmd->EndRenderPass();

    if (!IsCommandBufferOverriden())
    {
        GetGfxDriver()->ExecuteCommandBuffer(*cmd);
        cmd->Reset(true);
    }
}

bool RenderPipeline::FrameSetup(Gfx::CommandBuffer* cmd, Scene& scene, Camera& camera, float2 screenSize)
{
    auto AllocateImage = [](Gfx::CommandBuffer& cmd,
                            const Gfx::RG::ImageIdentifier& id,
                            glm::float2 size,
                            glm::float2 screenSize,
                            Gfx::GfxFormat format,
                            Gfx::RG::ImageDescription& desc)
    {
        if (size.x == 0)
        {
            desc.SetWidth(screenSize.x);
        }
        else if (size.x < 1.0f)
        {
            desc.SetWidth(screenSize.x * size.x);
        }
        else
            desc.SetWidth(size.x);
        if (size.y == 0)
        {
            desc.SetHeight(screenSize.y);
        }
        else if (size.y < 1.0f)
        {
            desc.SetHeight(screenSize.y * size.y);
        }
        else
            desc.SetHeight(size.y);
        desc.SetFormat(format);
        cmd.AllocateAttachment(id, desc);
    };

    mainColorDescription.SetRandomWrite(true);
    AllocateImage(*cmd, mainColor, {0, 0}, screenSize, Gfx::GfxFormat::R16G16B16A16_SFloat, mainColorDescription);
    AllocateImage(*cmd, mainDepth, {0, 0}, screenSize, Gfx::GfxFormat::D32_SFLOAT_S8_UInt, mainDepthDescription);
    AllocateImage(*cmd, depthCopy, {0, 0}, screenSize, Gfx::GfxFormat::D32_SFLOAT_S8_UInt, mainDepthDescription);

    // no settings quit here
    if (setting == nullptr)
    {
        finalColor = mainColor;
        return false;
    }

    UpdateSceneInfo(scene, camera, screenSize);
    renderingData.sceneInfo = &perScene.cpuParameter;
    renderingData.mainCamera = &camera;
    renderingData.mainColor = GetGfxDriver()->GetImageFromRenderGraph(mainColor);
    renderingData.mainDepth = GetGfxDriver()->GetImageFromRenderGraph(mainDepth);
    renderingData.depthCopy = GetGfxDriver()->GetImageFromRenderGraph(depthCopy);

    return true;
}

void RenderPipeline::UpdateSceneInfo(Scene& scene, Camera& camera, float2 screenSize)
{
    auto camGo = camera.GetGameObject();
    auto& renderingScene = scene.GetRenderingScene();
    auto sceneEnvironment = renderingScene.GetSceneEnvironment();

    glm::matrix<float, 4, 4> viewMatrix = camera.GetViewMatrix();
    glm::matrix<float, 4, 4> projectionMatrix = camera.GetAndUpdateProjectionMatrix(screenSize.x / screenSize.y);
    glm::matrix<float, 4, 4> vp = projectionMatrix * viewMatrix;
    glm::float4 viewPos = glm::float4(camGo->GetPosition(), 1);

    auto& param = perScene.cpuParameter;

    param.projection = projectionMatrix;
    param.viewProjection = vp;
    param.viewPos = viewPos;
    param.view = viewMatrix;
    param.shadowMapSize = {
        1024,
        1024,
        1.0 / 1024.0f,
        1.0 / 1024.0f,
    };
    param.invProjection = glm::inverse(projectionMatrix);
    param.invNDCToWorld = glm::inverse(viewMatrix) * glm::inverse(projectionMatrix);
    param.cameraZBufferParams = glm::vec4(
        camera.GetNear(),
        camera.GetFar(),
        (camera.GetNear() - camera.GetFar()) / (camera.GetNear() * camera.GetFar()),
        1.0f / camera.GetNear()
    );
    param.cameraFrustum = glm::vec4(
        -camera.GetProjectionRight(),
        camera.GetProjectionRight(),
        -camera.GetProjectionTop(),
        camera.GetProjectionTop()
    );
    param.screenSize = glm::vec4(screenSize.x, screenSize.y, 1.0f / screenSize.x, 1.0f / screenSize.y);
    param.time = Time::TimeSinceLaunch();
    if (sceneEnvironment)
    {
        auto coefs = sceneEnvironment->GetSkyboxProbeCoefficients();
        for (int i = 0; i < 9 && i < coefs.size(); ++i)
        {
            param.sh_2ndOrder.colors[i] = coefs[i];
        }
    }

    // light data
    {
        Light* mainLight = nullptr;
        ENGINE_BEGIN_PROFILE("Get Active Lights")
        auto lights = scene.GetActiveLights();
        ENGINE_END_PROFILE

        param.lightCount = glm::float4(lights.size(), 0, 0, 0);
        for (int i = 0; i < lights.size(); ++i)
        {
            param.lights[i].ambientScale = lights[i]->GetAmbientScale();
            param.lights[i].lightColor = glm::vec4(lights[i]->GetLightColor(), 1.0);
            param.lights[i].intensity = lights[i]->GetIntensity();
            auto model = lights[i]->GetGameObject()->GetWorldMatrix();
            switch (lights[i]->GetLightType())
            {
                case LightType::Directional:
                    {
                        mainLight = lights[i];
                        glm::vec3 pos = -glm::normalize(glm::vec3(model[2]));
                        param.lights[i].position = {pos, 0};

                        if (mainLight == nullptr || mainLight->GetIntensity() < lights[i]->GetIntensity())
                        {
                            mainLight = lights[i];
                        }
                        break;
                    }
                case LightType::Point:
                    {
                        glm::vec3 pos = model[3];
                        param.lights[i].position = {pos, 1};
                        param.lights[i].pointLightTerm1 = lights[i]->GetPointLightLinear();
                        param.lights[i].pointLightTerm2 = lights[i]->GetPointLightDistance();
                        break;
                    }
            }
        }

        if (mainLight)
        {
            state.renderMainLightShadow = mainLight->ShouldRenderShadowMap();

            param.worldToShadow = mainLight->WorldToShadowMatrix(camera.GetGameObject()->GetPosition());

            if (mainLight->IsShadowCacheEnabled())
            {
                param.cachedMainLightDirection = glm::vec4(mainLight->GetCachedLightDirection(), 1.0f);
            }
            else
            {
                param.cachedMainLightDirection = glm::vec4(mainLight->GetLightDirection(), 0.0f);
            }
        }
    }

    GetGfxDriver()->UploadBuffer(*perScene.gpuBuffer, (uint8_t*)&param, sizeof(GPUParameter::PerScene));
}

void RenderPipeline::BlitToFinalColor(Gfx::CommandBuffer* cmd)
{
    Gfx::RG::ImageIdentifier finalColorId = finalColor;

    bool isColorOverriden = renderConfig.colorOutputOverride.has_value();
    if (isColorOverriden)
    {
        finalColorId = *renderConfig.colorOutputOverride.value();
    }
}

Gfx::CommandBuffer* RenderPipeline::GetCommandBuffer()
{
    Gfx::CommandBuffer* cmd = commandBuffer.get();
    if (renderConfig.cmdOverride.has_value())
    {
        cmd = renderConfig.cmdOverride.value();
    }

    return cmd;
}

bool RenderPipeline::IsCommandBufferOverriden()
{
    return renderConfig.cmdOverride.has_value();
}

Gfx::RG::ImageIdentifier RenderPipeline::GetFinalColor()
{
    if (renderConfig.colorOutputOverride.has_value())
    {
        return *renderConfig.colorOutputOverride.value();
    }

    return finalColor;
}

RenderPipeline::CloudPass::CloudPass()
{
    volumetricCloud->SetShader(ShaderLibrary::GetShader(volumetricCloudShader));
}

void RenderPipeline::CloudPass::Execute(Cloud& cloud, Gfx::CommandBuffer& cmd, RenderingData& renderingData)
{
    volumetricCloud->CopyProperties(*cloud.volumetricCloud);
    volumetricCloud->SetTexture("cloudDensity", cloud.cloudNoise.baseShapeNoise.get());
    volumetricCloud->SetTexture("highFrequencyCloudDensity", cloud.cloudNoise.highFrequencyNoise.get());

    volumetricCloud->SetTexture("mainColor", renderingData.mainColor);
    volumetricCloud
        ->SetTexture("depthMap", renderingData.depthCopy, Gfx::ImageViewOption{0, 1, 0, 1, Gfx::ImageAspect::Depth});
    volumetricCloud->SetTexture("interleavedGradientNoise", renderingData.interleavedGradientNoise.GetNoiseTexture());
    cmd.BindShaderProgram(volumetricCloud->GetShader()->GetShaderProgram(), volumetricCloud->GetShaderConfig());
    cmd.BindResource(
        volumetricCloud->GetSet(Gfx::DescriptorSetSemantics::Material),
        volumetricCloud->GetShaderResource()
    );
    cmd.Dispatch((renderingData.sceneInfo->screenSize.x + 7) / 8, (renderingData.sceneInfo->screenSize.y + 7) / 8, 1);
}

} // namespace Rendering
  //
