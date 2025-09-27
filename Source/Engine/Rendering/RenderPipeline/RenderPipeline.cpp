#include "RenderPipeline.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Component/ParticleSystem.hpp"
#include "Core/Component/ReflectionProbe.hpp"
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

using namespace Rendering::Passes;
namespace Rendering
{
RenderPipeline::RenderPipeline()
{
    particleRenderer = std::make_unique<ParticleRenderer>();
    shadowRenderer = std::make_unique<ShadowRenderer>();
    shadowRenderer->Init();
    reflectionProbeUpdate =
        std::make_unique<ReflectionProbeUpdate>(perScene.scene.get(), perScene.mainLightShadow.get());

    commandBuffer = GetGfxDriver()->CreateCommandBuffer();

    Gfx::SubpassAttachment skyboxOnlyPassAttachment[] = {
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
    renderingData.screenSize = screenSize;
    renderingData.screenAspect = screenSize.x / screenSize.y;
    cmd->BeginLabel("Render Scene", {0.623, 0.323, 0.4123, 1.0f});
    if (!FrameSetup(cmd, scene, camera, screenSize))
    {
        return;
    }

    // Setup
    renderingData.renderPipelineSettings = setting.Get();
    renderingData.cameraFrustum = camera.GetFrustum(renderingData.screenAspect);
    glm::float2 mainRTSize = {mainColorDescription.GetWidth(), mainColorDescription.GetHeight()};

    auto& renderingScene = scene.GetRenderingScene();

    ENGINE_BEGIN_PROFILE("Bulid Scene Draw List");
    DrawList sceneDrawList{};
    if (setting->frustumCull)
    {
        auto renderers = renderingScene.QueryRendererInFrustum(renderingData.cameraFrustum);
        sceneDrawList.Add(renderers);
    }
    else
    {
        sceneDrawList.Add(renderingScene.GetMeshRenderers());
    }

    sceneDrawList.Lock();

    ENGINE_BEGIN_PROFILE("Sort");
    sceneDrawList.Sort(camera.GetGameObject()->GetPosition());
    ENGINE_END_PROFILE; // Sort

    ENGINE_END_PROFILE; // Bulid Scene Draw List

    ENGINE_END_PROFILE; // RenderPipeline - Setup

    // Reflection Probe Updateo

    auto reflectionProbes = renderingScene.GetRenderingObjects<ReflectionProbe>();
    for (RenderingObjectBase* r : reflectionProbes)
    {
        ReflectionProbe* reflectionProbe = static_cast<ReflectionProbe*>(r);
        reflectionProbeUpdate->Execute(*cmd, renderingData, *reflectionProbe);
    }

    cmd->BindResource(0, perScene.globalResource.get());

    // Shadow Pass
    ENGINE_BEGIN_PROFILE("Shadow")
    shadowRenderer->Execute(*cmd, renderingData);
    ENGINE_END_PROFILE; // Shadow

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
        Gfx::ClearValue clears[] = {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {1.0f, 1.0f, 1.0f, 1.0f}, {0, 0}};
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
    cmd->EndLabel(); // GBuffer

    auto downSampledDepthCopyDesc = mainDepthDescription;
    downSampledDepthCopyDesc.SetFormat(Gfx::GfxFormat::R32_SFloat);
    downSampledDepthCopyDesc.SetWidth(mainRTSize.x / 2);
    downSampledDepthCopyDesc.SetHeight(mainRTSize.y / 2);
    downSampledDepthCopyDesc.SetRandomWrite(true);
    cmd->AllocateAttachment(downSampledDepthCopy, downSampledDepthCopyDesc);
    depthDownSamplerPass.Setup(mainDepth, downSampledDepthCopy, downSampledDepthCopyDesc);
    depthDownSamplerPass.Execute(*cmd);

    cmd->BindResource(0, perScene.globalResource.get());

    // ssao pass
    ssaoPass.Execute(cmd, downSampledDepthCopy, mainDepth, mainDepthDescription, setting);

    // Contact Shadow (directional main light only) - BEFORE shading so future shaders can consume
    {
        Light* mainLight = nullptr;
        if (renderingData.mainLightIndex >= 0 && renderingData.mainLightIndex < renderingData.lights.size())
            mainLight = renderingData.lights[renderingData.mainLightIndex];
        if (mainLight)
        {
            // Ensure we have an up-to-date depthCopy for compute sampling
            contactShadowPass
                .Execute(*cmd, renderingData, mainLight, GetGfxDriver()->GetImageFromRenderGraph(mainDepth));
        }
    }

    // Shading
    cmd->BeginLabel("Shading", &labelColors.passColor[0]);
    {
        cmd->BindResource(0, perScene.globalResource.get());
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

        Gfx::ClearValue lightingPassClearValues[] = {{0, 0, 0, 0}, {1, 0}};
        auto shadingShader = shadingPass.shadingShader->GetShaderProgram();
        auto diffuseCube = camera.GetDiffuseEnv();
        auto specularCube = camera.GetDiffuseEnv();

        auto depthImage = GetGfxDriver()->GetImageFromRenderGraph(depthCopy);
        auto& depthImageView = depthImage->GetImageView({Gfx::ImageAspect::Depth});

        auto contactShadowMap = contactShadowPass.GetOutputId();

        shadingPass.gpuResource->SetImage("contactShadowMap"_shaderBinding, contactShadowMap);
        shadingPass.gpuResource->SetImage("albedoTex"_shaderBinding, albedoGBuffer);
        shadingPass.gpuResource->SetImage("normalTex"_shaderBinding, normalGBuffer);
        shadingPass.gpuResource->SetImage("maskTex"_shaderBinding, maskGBuffer);
        shadingPass.gpuResource->SetImage("depthTex"_shaderBinding, &depthImageView);
        shadingPass.gpuResource->SetImage("shadowMap"_shaderBinding, shadowRenderer->GetShadowMap());
        shadingPass.gpuResource->SetImage("ambientOcclusion"_shaderBinding, ssaoPass.GetSSAOTex());
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
    cmd->EndLabel(); // Shading

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
            cmd->EndLabel(); // Draw Graphics
        }

        skyboxPass.Execute(cmd);

        cmd->EndRenderPass();

        auto& renderingScene = scene.GetRenderingScene();
        auto clouds = renderingScene.GetClouds();
        if (!clouds.empty())
        {
            cloudPass.Execute(*clouds[0], *cmd, renderingData);
        }

        // draw objects
        cmd->BeginLabel("Forward Objects", {0.12, 0.64, 0.342, 1.0f});
        cmd->BeginRenderPass(forwardPass.pass, clears);
        sceneDrawList.DrawRangeHelper(*cmd, sceneDrawList.transparentIndex, sceneDrawList.size());
        cmd->EndLabel(); // Forward Objects

        // draw particles
        cmd->BindResource(0, GetPerSceneGPUResource());
        cmd->BeginLabel("Particles", {0.55, 0.11, 0.57, 1.0f});
        auto particleSystems = scene.GetRenderingScene().GetParticleSystems();
        for (auto p : particleSystems)
        {
            particleRenderer->Draw(*cmd, p->GetDraw());
        }
        cmd->EndLabel(); // Particles

        cmd->EndRenderPass();
    }
    cmd->EndLabel(); // Forward

    // start post procesing
    finalColor = mainColor;

    if (setting->postProcess.colorGrading)
    {
        auto shader = colorGradingPass.colorGradingShader->GetShaderProgram();
        cmd->BeginLabel("Color Grading", &labelColors.passColor[0]);
        // TODO
        Gfx::RenderImageDescriptor resultDesc(mainRTSize.x, mainRTSize.y, Gfx::GfxFormat::R8G8B8A8_SRGB);
        cmd->AllocateAttachment(colorGradingPass.colorGradingId, resultDesc);
        colorGradingPass.pass.SetAttachment(0, colorGradingPass.colorGradingId);
        colorGradingPass.mat.SetTexture("mainColor", renderingData.mainColor);
        Gfx::ClearValue clears[] = {{0, 0, 0, 0}};
        cmd->BeginRenderPass(colorGradingPass.pass, clears);
        cmd->BindShaderProgram(shader, shader->GetDefaultShaderConfig());
        cmd->BindResource(0, colorGradingPass.mat.GetShaderResource());
        cmd->Draw(6, 1, 0, 0);
        cmd->EndRenderPass();
        finalColor = colorGradingPass.colorGradingId;
        cmd->EndLabel(); // Color Grading
    }

    // FXAA
    if (setting->fxaa)
    {
        cmd->BeginLabel("FXAA", &labelColors.passColor[0]);
        {
            Gfx::RenderImageDescriptor resultDesc(mainRTSize.x, mainRTSize.y, Gfx::GfxFormat::R8G8B8A8_SRGB);
            cmd->AllocateAttachment(fxaaPass.fxaaId, resultDesc);
            fxaaPass.Execute(*cmd, {mainRTSize.x, mainRTSize.y, 0, 0}, finalColor, fxaaPass.fxaaId);
            finalColor = fxaaPass.fxaaId;
        }
        cmd->EndLabel(); // FXAA
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
    globalResource = GetGfxDriver()->CreateShaderResource();

    scene = GetGfxDriver()->CreateBuffer(sizeof(GPUParameter::Scene), Gfx::BufferUsage::Uniform, false, false, "Scene");
    camera =
        GetGfxDriver()->CreateBuffer(sizeof(GPUParameter::Camera), Gfx::BufferUsage::Uniform, false, false, "Camera");
    mainLightShadow = GetGfxDriver()->CreateBuffer(
        sizeof(GPUParameter::MainLightShadow),
        Gfx::BufferUsage::Uniform,
        false,
        false,
        "MainLightShadow"
    );
    globalResource->SetBuffer("scene", scene.get());
    globalResource->SetBuffer("camera", camera.get());
    globalResource->SetBuffer("mainLightShadow", mainLightShadow.get());
}

RenderPipeline::ShadingPass::ShadingPass()
{
    pass = Gfx::RenderPass("shading", 1, 2);
    Gfx::SubpassAttachment lightingPassAttachment{
        0,
        Gfx::AttachmentLoadOperation::Load,
        Gfx::AttachmentStoreOperation::Store
    };

    Gfx::SubpassAttachment depthAttachment{
        1,
        Gfx::AttachmentLoadOperation::Load,
        Gfx::AttachmentStoreOperation::Store,
        Gfx::AttachmentLoadOperation::Load,
        Gfx::AttachmentStoreOperation::DontCare,
    };
    Gfx::SubpassAttachment lightingPassAttachments[] = {lightingPassAttachment};
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
    pass = Gfx::RenderPass("gbuffer", 1, 5);
    Gfx::SubpassAttachment lighting{0, Gfx::AttachmentLoadOperation::Clear, Gfx::AttachmentStoreOperation::Store};
    Gfx::SubpassAttachment albedo{1};
    Gfx::SubpassAttachment normal{2};
    Gfx::SubpassAttachment property{3};
    Gfx::SubpassAttachment depth{4};
    Gfx::SubpassAttachment subpassAttachments[] = {lighting, albedo, normal, property};
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

    Gfx::SubpassAttachment attachmentDesc{0, Gfx::AttachmentLoadOperation::Load, Gfx::AttachmentStoreOperation::Store};
    Gfx::SubpassAttachment attachments[] = {attachmentDesc};
    pass.SetSubpass(0, attachments);
}

RenderPipeline::ForwardPass::ForwardPass()
{
    pass = Gfx::RenderPass::Default(
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
    const Gfx::ImageIdentifier& src,
    const Gfx::ImageIdentifier& dst
)
{
    pass.SetAttachment(0, dst);
    resource->SetImage("source", GetGfxDriver()->GetImageFromRenderGraph(src));
    Gfx::ClearValue clears[] = {{0, 0, 0, 0}};
    cmd.BeginRenderPass(dst);
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

    cmd->BindResource(0, perScene.globalResource.get());

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
                            const Gfx::ImageIdentifier& id,
                            glm::float2 size,
                            glm::float2 screenSize,
                            Gfx::GfxFormat format,
                            Gfx::RenderImageDescriptor& desc)
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

    renderingData.gpuCamera = &perScene.cameraParameter;
    renderingData.gpuScene = &perScene.sceneParameter;
    renderingData.gpuMainLightShadow = &perScene.mainLightShadowParameter;
    renderingData.mainCamera = &camera;
    renderingData.mainColor = GetGfxDriver()->GetImageFromRenderGraph(mainColor);
    renderingData.mainDepth = GetGfxDriver()->GetImageFromRenderGraph(mainDepth);
    renderingData.depthCopy = GetGfxDriver()->GetImageFromRenderGraph(depthCopy);
    renderingData.scene = &scene;
    UpdateSceneInfo(scene, camera, screenSize);
    return true;
}

void RenderPipeline::UpdateSceneInfo(Scene& scene, Camera& camera, float2 screenSize)
{
    auto camGo = camera.GetGameObject();
    auto& renderingScene = scene.GetRenderingScene();
    auto sceneEnvironment = renderingScene.GetSceneEnvironment();

    auto& cameraParam = perScene.cameraParameter;
    auto& sceneParam = perScene.sceneParameter;
    auto& mainLightShadowParam = perScene.mainLightShadowParameter;
    cameraParam = RenderingUtils::CreateCameraGPUParameter(camera, screenSize);

    // update main light shadow parameters
    auto shadowMapTexelSize = shadowRenderer->GetShadowMapTexelSize();
    mainLightShadowParam.shadowMapSize = {
        shadowMapTexelSize.z,
        shadowMapTexelSize.w,
        shadowMapTexelSize.x,
        shadowMapTexelSize.y,
    };

    // update scene parameters
    sceneParam.time = Time::TimeSinceLaunch();
    {
        Light* mainLight = nullptr;
        ENGINE_BEGIN_PROFILE("Get Active Lights")
        renderingData.lights = scene.GetActiveLights();
        renderingData.mainLightIndex = -1;
        ENGINE_END_PROFILE
        auto& lights = renderingData.lights;

        sceneParam.lightCount = lights.size();
        for (int i = 0; i < lights.size(); ++i)
        {
            sceneParam.lights[i].ambientScale = lights[i]->GetAmbientScale();
            sceneParam.lights[i].lightColor = glm::vec4(lights[i]->GetLightColor(), 1.0);
            sceneParam.lights[i].intensity = lights[i]->GetIntensity();
            auto model = lights[i]->GetGameObject()->GetWorldMatrix();
            switch (lights[i]->GetLightType())
            {
                case LightType::Directional:
                    {
                        mainLight = lights[i];
                        renderingData.mainLightIndex = i;
                        sceneParam.lights[i].position = {-mainLight->GetLightDirection(), 0};

                        if (mainLight == nullptr || mainLight->GetIntensity() < lights[i]->GetIntensity())
                        {
                            mainLight = lights[i];
                            renderingData.mainLightIndex = i;
                        }
                        break;
                    }
                case LightType::Point:
                    {
                        glm::vec3 pos = model[3];
                        sceneParam.lights[i].position = {pos, 1};
                        sceneParam.lights[i].pointLightTerm1 = lights[i]->GetPointLightLinear();
                        sceneParam.lights[i].pointLightTerm2 = lights[i]->GetPointLightDistance();
                        break;
                    }
            }
        }

        if (mainLight)
        {
            state.renderMainLightShadow = mainLight->ShouldRenderShadowMap();

            mainLightShadowParam.worldToShadow = shadowRenderer->GetShadowToWorldMatrix(renderingData);

            if (mainLight->IsShadowCacheEnabled())
            {
                mainLightShadowParam.cachedMainLightDirection = glm::vec4(mainLight->GetCachedLightDirection(), 1.0f);
            }
            else
            {
                mainLightShadowParam.cachedMainLightDirection = glm::vec4(mainLight->GetLightDirection(), 0.0f);
            }
        }
    }

    GetGfxDriver()->UploadBuffer(*perScene.camera, (uint8_t*)&cameraParam, sizeof(GPUParameter::Camera));
    GetGfxDriver()->UploadBuffer(*perScene.scene, (uint8_t*)&sceneParam, sizeof(GPUParameter::Scene));
    GetGfxDriver()->UploadBuffer(
        *perScene.mainLightShadow,
        (uint8_t*)&mainLightShadowParam,
        sizeof(GPUParameter::MainLightShadow)
    );
}

void RenderPipeline::BlitToFinalColor(Gfx::CommandBuffer* cmd)
{
    Gfx::ImageIdentifier finalColorId = finalColor;

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

const Gfx::ImageIdentifier& RenderPipeline::GetOutputColor()
{
    Gfx::ImageIdentifier debugImage;
    Gfx::ImageIdentifier finalColorId;
    if (ssaoPass.DebugBlit(debugImage))
    {
        finalColor = debugImage;
    }

    return finalColor;
}

Gfx::ImageIdentifier RenderPipeline::GetFinalColor()
{
    if (renderConfig.colorOutputOverride.has_value())
    {
        return *renderConfig.colorOutputOverride.value();
    }

    Gfx::ImageIdentifier debugImage;
    Gfx::ImageIdentifier finalColorId;
    if (ssaoPass.DebugBlit(debugImage))
    {
        finalColorId = debugImage;
    }
    else
        Gfx::ImageIdentifier finalColorId = finalColor;

    return finalColorId;
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
    cmd.Dispatch((renderingData.gpuCamera->screenSize.x + 7) / 8, (renderingData.gpuCamera->screenSize.y + 7) / 8, 1);
}

} // namespace Rendering
  //
