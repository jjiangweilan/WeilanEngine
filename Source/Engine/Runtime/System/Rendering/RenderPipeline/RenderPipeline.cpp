#include "RenderPipeline.hpp"
#include "Engine/Core/Profiler/Profiler.hpp"
#include "Engine/Core/Time.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Runtime/Module/Ocean/OceanComponent.hpp"
#include "Engine/Runtime/Object/Component/ParticleSystem.hpp"
#include "Engine/Runtime/Object/Component/ReflectionProbe.hpp"
#include "Engine/Runtime/Object/Component/SceneEnvironment.hpp"
#include "Engine/Runtime/Object/Texture/Texture.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetDatabase.hpp"
#include "Engine/Runtime/System/Rendering/Graphics.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/Passes/ReflectionProbeUpdate.hpp"
#include "Engine/Runtime/System/Rendering/Renderers/ParticleRenderer.hpp"
#include "Engine/Runtime/System/Rendering/RenderingUtils.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"
#include "Engine/Runtime/System/SceneManager/Scene.hpp"

using namespace Rendering::Passes;
namespace Rendering
{
RenderPipeline::RenderPipeline()
{
    particleRenderer = std::make_unique<ParticleRenderer>();
    shadowRenderer = std::make_unique<ShadowRenderer>();
    fogPass = std::make_unique<Passes::FogPass>();
    shadowRenderer->Init();

    reflectionProbeUpdate = AddRenderPipelinePass<ReflectionProbeUpdate>();
    shadingPass = AddRenderPipelinePass<Passes::ShadingPass>();
    cloudPass = AddRenderPipelinePass<Passes::CloudPass>();
    colorGradingPass = AddRenderPipelinePass<Passes::ColorGradingPass>();
    fxaaPass = AddRenderPipelinePass<Passes::FXAAPass>();
    // rayTracingTestPass = AddRenderPipelinePass<Passes::RayTracingTestPass>();
    screenSpaceShadowPass = AddRenderPipelinePass<Passes::ScreenSpaceShadowPass>();
    ssaoPass = AddRenderPipelinePass<Passes::SSAO>();
    ssilPass = AddRenderPipelinePass<Passes::SSIL>();
    lightingCombinePass = AddRenderPipelinePass<Passes::LightingCombinePass>();
    bloomPass = AddRenderPipelinePass<Passes::BloomPass>();
    depthDownSamplerPass = AddRenderPipelinePass<Passes::DepthDownSampler>();
    staticMotionVectorPass = AddRenderPipelinePass<Passes::StaticMotionVectorPass>();
    hierarchyZBufferPass = AddRenderPipelinePass<Passes::HierarchyZBufferPass>();
    skyboxPass = AddRenderPipelinePass<SkyboxPass>();
    contactShadowPass = AddRenderPipelinePass<ContactShadowPass>();

    commandBuffer = GetGfxDriver()->CreateCommandBuffer();

    Gfx::SubpassAttachment skyboxOnlyPassAttachment[] = {
        {0, Gfx::AttachmentLoadOperation::Clear, Gfx::AttachmentStoreOperation::Store}
    };
    skyboxOnlyPass.SetSubpass(0, skyboxOnlyPassAttachment);

    bufferAllocator = std::make_unique<PipelineGPUBufferAllocator>();
    renderingData.pipelineAllocator = bufferAllocator.get();
    renderingData.perScene = &perScene;

    for (auto& p : renderPipelinePasses)
    {
        p->OnInit(&renderingData);
    }
}

RenderPipeline::~RenderPipeline() {}

void RenderPipeline::Render(Scene& scene, Camera& camera, glm::float2 screenSize)
{
    ENGINE_BEGIN_PROFILE("RenderPipeline - Setup")
    setting = scene.GetRenderPipelineSetting();
    Gfx::CommandBuffer* cmd = GetCommandBuffer();
    renderingData.screenSize = screenSize;
    renderingData.screenAspect = screenSize.x / screenSize.y;
    renderingData.globalResource = perScene.globalResource.get();
    renderingData.perScene = &perScene;
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

    // auto reflectionProbes = renderingScene.GetRenderingObjects<ReflectionProbe>();
    // for (RenderingObjectBase* r : reflectionProbes)
    // {
    //     ReflectionProbe* reflectionProbe = static_cast<ReflectionProbe*>(r);
    // }
    reflectionProbeUpdate->Execute(*cmd, renderingData);
    renderingData.specularCubemap = reflectionProbeUpdate->GetIBLCubemap();

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
        Gfx::RenderAttachment gbufferAttachments[] = {
            {mainColor, Gfx::AttachmentLoadOperation::Clear},
            {albedoGBuffer, Gfx::AttachmentLoadOperation::Clear},
            {normalGBuffer, Gfx::AttachmentLoadOperation::Clear},
            {maskGBuffer, Gfx::AttachmentLoadOperation::Clear},
            {mainDepth, Gfx::AttachmentLoadOperation::Clear}
        };
        Gfx::ClearValue clears[] = {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {1.0f, 1.0f, 1.0f, 1.0f}, {0, 0}};
        cmd->BeginRenderPass(gbufferAttachments, clears);

        // draw
        std::optional<Gfx::PolygonMode> polygonMode = setting->debugDraw.wireframe ? std::optional<Gfx::PolygonMode>(Gfx::PolygonMode::Line) : std::nullopt;
        sceneDrawList.DrawRangeHelper(*cmd, 0, sceneDrawList.alphaTestIndex, polygonMode);
        sceneDrawList.DrawRangeHelper(*cmd, sceneDrawList.alphaTestIndex, sceneDrawList.transparentIndex, polygonMode);

        cmd->EndRenderPass();
    }
    cmd->EndLabel(); // GBuffer

    staticMotionVectorPass->Execute(*cmd, mainDepth, mainDepthDescription, renderingData);
    hierarchyZBufferPass->Execute(*cmd, mainDepth, mainDepthDescription, renderingData);

    auto downSampledDepthCopyDesc = mainDepthDescription;
    downSampledDepthCopyDesc.SetFormat(Gfx::GfxFormat::R32_SFloat);
    downSampledDepthCopyDesc.SetWidth(mainRTSize.x / 2);
    downSampledDepthCopyDesc.SetHeight(mainRTSize.y / 2);
    downSampledDepthCopyDesc.SetRandomWrite(true);
    cmd->AllocateAttachment(downSampledDepthCopy, downSampledDepthCopyDesc);
    depthDownSamplerPass->Setup(mainDepth, downSampledDepthCopy, downSampledDepthCopyDesc);
    depthDownSamplerPass->Execute(*cmd);

    cmd->BindResource(0, perScene.globalResource.get());

    // ssao pass
    ssaoPass->Execute(cmd, hierarchyZBufferPass->GetOutputId(), downSampledDepthCopy, mainDepth, mainDepthDescription, setting, renderingData);

    // Contact Shadow (directional main light only) - BEFORE shading so future shaders can consume
    {
        Light* mainLight = nullptr;
        if (renderingData.mainLightIndex >= 0 && renderingData.mainLightIndex < renderingData.lights.size())
            mainLight = renderingData.lights[renderingData.mainLightIndex];
        if (mainLight)
        {
            // Ensure we have an up-to-date depthCopy for compute sampling
            contactShadowPass
                ->Execute(*cmd, renderingData, mainLight, GetGfxDriver()->GetImageFromRenderGraph(mainDepth));
        }
    }

    // Shading
    cmd->BeginLabel("Shading", &labelColors.passColor[0]);
    {
        cmd->BindResource(0, perScene.globalResource.get());
        // Upload GPU Parameter
        shadingPass->UploadGPUParameter(
            shadowRenderer->GetShadowMapTexelSize(),
            setting->shadowMap.constantBias / 1000.0f,
            setting->shadowMap.normalBias
        );

        cmd->Blit(mainDepth, depthCopy);

        Gfx::ClearValue lightingPassClearValues[] = {{0, 0, 0, 0}, {1, 0}};
        auto diffuseCube = &reflectionProbeUpdate->GetIBLCubemap()->GetImageView(Gfx::ImageViewOption{5, 1, 0, 6, Gfx::ImageAspect::Color, Gfx::ImageViewOption::Type::Cubemap});
        auto specularCube = &reflectionProbeUpdate->GetIBLCubemap()->GetDefaultImageView();

        auto depthImage = GetGfxDriver()->GetImageFromRenderGraph(depthCopy);
        auto& depthImageView = depthImage->GetImageView({Gfx::ImageAspect::Depth});

        Gfx::RenderAttachment lightingPassAttachments[] = {
            {mainColor, Gfx::AttachmentLoadOperation::Load},
            {mainDepth, Gfx::AttachmentLoadOperation::Load}
        };
        cmd->BeginRenderPass(lightingPassAttachments, lightingPassClearValues);
        shadingPass->Execute(
            *cmd,
            albedoGBuffer,
            normalGBuffer,
            maskGBuffer,
            &depthImageView,
            &shadowRenderer->GetShadowMap()->GetDefaultImageView(),
            &ssaoPass->GetSSAOTex(),
            &contactShadowPass->GetOutputId(),
            diffuseCube,
            specularCube,
            renderingData
        );

        cmd->EndRenderPass();

        cmd->Blit(mainColor, colorCopy);
    }
    cmd->EndLabel(); // Shading

    // ssil pass
    if (setting->ssil.enabled)
    {
        ssilPass->Execute(cmd, colorCopy, mainDepth, albedoGBuffer, normalGBuffer, mainColor, setting.Get(), renderingData);
        lightingCombinePass->Execute(cmd, ssilPass->GetOutputId(), albedoGBuffer, mainColor, renderingData);
    }

    // TODO: copy mainColor and mainDepth for special effects

    // Forward Pass
    cmd->BeginLabel("Forward", &labelColors.passColor[0]);
    {

        Gfx::RenderAttachment forwardPassAttachments[] = {
            {mainColor, Gfx::AttachmentLoadOperation::Load},
            {mainDepth, Gfx::AttachmentLoadOperation::Load}
        };
        Gfx::ClearValue clears[] = {{0, 0, 0, 0}, {0, 0}};
        cmd->BeginRenderPass(forwardPassAttachments, clears);
        cmd->BindResource(0, perScene.globalResource.get());

        ExecuteRenderEvents(*cmd, scene, RenderEvents::ForwardOpaque);

        if (renderConfig.drawGraphics)
        {
            cmd->BeginLabel("Draw Graphics", &labelColors.passColor[0]);
            RenderingUtils::DrawGraphics(*cmd);
            cmd->EndLabel(); // Draw Graphics
        }

        skyboxPass->Execute(cmd);

        cmd->EndRenderPass();

        auto& renderingScene = scene.GetRenderingScene();
        auto clouds = renderingScene.GetClouds();
        if (!clouds.empty())
        {
            cloudPass->Execute(*clouds[0], *cmd, renderingData);
        }

        // draw objects
        cmd->BeginLabel("Forward Objects", {0.12, 0.64, 0.342, 1.0f});
        cmd->BeginRenderPass(forwardPassAttachments, clears);
        sceneDrawList.DrawRangeHelper(*cmd, sceneDrawList.transparentIndex, sceneDrawList.size(), setting->debugDraw.wireframe ? std::optional<Gfx::PolygonMode>(Gfx::PolygonMode::Line) : std::nullopt);

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

    // Fog
    fogPass->Execute(*cmd, mainColor, depthCopy, renderingScene.GetSceneEnvironmentData().fogPassParameters);

    // Ray Tracing Test
    // rayTracingTestPass->Execute(
    //     *cmd,
    //     depthCopy,
    //     scene.GetRenderingScene().GetRayTracingSceneHandle(),
    //     scene.GetRenderingScene().GetRayTracingContext(),
    //     GetPerSceneGPUResource(),
    //     mainRTSize
    // );

    if (setting->postProcess.bloom.enabled)
    {
        bloomPass->Execute(*cmd, mainColor, mainColorDescription, setting->postProcess.bloom, renderingData);
    }

    // start post procesing
    finalColor = mainColor;

    if (setting->postProcess.colorGrading)
    {
        cmd->BeginLabel("Color Grading", &labelColors.passColor[0]);
        colorGradingPass->Execute(*cmd, renderingData.mainColor, mainRTSize);
        finalColor = colorGradingPass->GetOutputId();
        cmd->EndLabel(); // Color Grading
    }

    // FXAA
    if (setting->fxaa)
    {
        cmd->BeginLabel("FXAA", &labelColors.passColor[0]);
        {
            Gfx::RenderImageDescriptor resultDesc(mainRTSize.x, mainRTSize.y, Gfx::GfxFormat::R8G8B8A8_SRGB);
            cmd->AllocateAttachment(fxaaPass->GetOutputId(), resultDesc);
            fxaaPass->Execute(*cmd, {mainRTSize.x, mainRTSize.y, 0, 0}, finalColor, fxaaPass->GetOutputId());
            finalColor = fxaaPass->GetOutputId();
        }
        cmd->EndLabel(); // FXAA
    }

    cmd->EndLabel(); // Render Scene

    if (!IsCommandBufferOverriden())
    {
        GetGfxDriver()->ExecuteCommandBuffer(*cmd);
        cmd->Reset(true);
    }

    // Update camera temporal state for the next frame
    camera.SetPreviousViewProjection(perScene.cameraParameter.viewProjection);
    camera.SetInvPreviousViewProjection(perScene.cameraParameter.invNDCToWorld);
}

PerScene::PerScene()
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
    skyboxPass->Execute(cmd);
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
    AllocateImage(*cmd, colorCopy, {0, 0}, screenSize, Gfx::GfxFormat::R16G16B16A16_SFloat, mainColorDescription);
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
    renderingData.colorCopy = GetGfxDriver()->GetImageFromRenderGraph(colorCopy);
    renderingData.scene = &scene;
    UpdateSceneInfo(scene, camera, screenSize);
    return true;
}

void RenderPipeline::UpdateSceneInfo(Scene& scene, Camera& camera, float2 screenSize)
{
    auto& cameraParam = perScene.cameraParameter;
    auto& sceneParam = perScene.sceneParameter;
    auto& mainLightShadowParam = perScene.mainLightShadowParameter;
    cameraParam = RenderingUtils::CreateCameraGPUParameter(camera, screenSize);

    // populate previous matrices from camera component
    cameraParam.previousViewProjection = camera.GetPreviousViewProjection();
    cameraParam.invPreviousViewProjection = camera.GetInvPreviousViewProjection();

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

            auto mainLight = renderingData.GetMainLight();

            if (mainLight)
            {
                shadowRenderer->Setup(
                    *mainLight,
                    renderingData
                );

                if (mainLight->IsCascadeShadowEnabled())
                {
                    int cascadeCount = mainLight->GetCascadeCount();
                    mainLightShadowParam.shadowCascadeCount = mainLight->GetCascadeCount();

                    for (int i = 0; i < cascadeCount; ++i)
                    {
                        mainLightShadowParam.worldToShadow[i] = shadowRenderer->GetWorldToShadowMatrix(*mainLight, renderingData, mainLight->GetShadowCascadeSplits()[i].splitDistance);
                        mainLightShadowParam.shadowDistances[i].distance = mainLight->GetShadowCascadeSplits()[i].splitDistance;
                    }
                }
                else
                {
                    mainLightShadowParam.worldToShadow[0] = shadowRenderer->GetWorldToShadowMatrix(*mainLight, renderingData, mainLight->GetShadowDistance());
                    mainLightShadowParam.shadowCascadeCount = 1.0;
                    mainLightShadowParam.shadowDistances[0].distance = mainLight->GetShadowDistance();
                }

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
    finalColorId = finalColor;

    for (auto& pass : renderPipelinePasses)
    {
        if (pass->DebugBlit(debugImage))
        {
            finalColorId = debugImage;
            break;
        }
    }

    return finalColorId;
}

Gfx::ImageIdentifier RenderPipeline::GetFinalColor()
{
    if (renderConfig.colorOutputOverride.has_value())
    {
        return *renderConfig.colorOutputOverride.value();
    }

    return finalColor;
}

void RenderPipeline::ExecuteRenderEvents(Gfx::CommandBuffer& cmd, Scene& scene, RenderEvents event)
{
    RenderingObjectList::ObjectList renderingObjects = scene.GetRenderingScene().GetRenderingObjectsByEvent(event);

    for (auto obj : renderingObjects)
    {
        obj->Render(cmd, renderingData);
    }
}

} // namespace Rendering
  //
