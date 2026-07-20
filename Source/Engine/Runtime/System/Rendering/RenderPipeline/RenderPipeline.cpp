#include "RenderPipeline.hpp"
#include "Engine/Core/Profiler/Profiler.hpp"
#include "Engine/Core/Time.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/MiddleLayer/EngineInternalResources.hpp"
#include "Engine/Runtime/Module/Ocean/OceanComponent.hpp"
#include "Engine/Runtime/Object/Component/MeshRenderer.hpp"
#include "Engine/Runtime/Object/Component/ParticleSystem.hpp"
#include "Engine/Runtime/Object/Component/ReflectionProbe.hpp"
#include "Engine/Runtime/Object/Component/SceneEnvironment.hpp"
#include "Engine/Runtime/Object/Texture/Texture.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetDatabase.hpp"
#include "Engine/Runtime/System/Rendering/GPUDriven/GPUDrivenManager.hpp"
#include "Engine/Runtime/System/Rendering/Graphics.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/Passes/ReflectionProbeUpdate.hpp"
#include "Engine/Runtime/System/Rendering/Renderers/ParticleRenderer.hpp"
#include "Engine/Runtime/System/Rendering/RenderingUtils.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"
#include "Engine/Runtime/System/SceneManager/Scene.hpp"
#include <algorithm>
#include <bit>

using namespace Rendering::Passes;
namespace Rendering
{
RenderPipeline::RenderPipeline()
{
    grassSurfaceRenderer = std::make_unique<GrassSurfaceRenderer>();
    particleRenderer = std::make_unique<ParticleRenderer>();
    shadowRenderer = std::make_unique<ShadowRenderer>();
    pointLightShadowRenderer = std::make_unique<PointLightShadowRenderer>();
    fogPass = std::make_unique<Passes::FogPass>();
    shadowRenderer->Init();
    pointLightShadowRenderer->Init();

    reflectionProbeUpdate = AddRenderPipelinePass<ReflectionProbeUpdate>();
    shadingPass = AddRenderPipelinePass<Passes::ShadingPass>();
    cloudPass = AddRenderPipelinePass<Passes::CloudPass>();
    colorGradingPass = AddRenderPipelinePass<Passes::ColorGradingPass>();
    fxaaPass = AddRenderPipelinePass<Passes::FXAAPass>();
    taaPass = AddRenderPipelinePass<Passes::TAAPass>();
    screenSpaceShadowPass = AddRenderPipelinePass<Passes::ScreenSpaceShadowPass>();
    ssaoPass = AddRenderPipelinePass<Passes::SSAO>();
    ssilPass = AddRenderPipelinePass<Passes::SSIL>();
    rtgiPass = AddRenderPipelinePass<Passes::RTGI>();
    giPass = AddRenderPipelinePass<Passes::GI>();
    lightingCombinePass = AddRenderPipelinePass<Passes::LightingCombinePass>();
    bloomPass = AddRenderPipelinePass<Passes::BloomPass>();
    depthDownSamplerPass = AddRenderPipelinePass<Passes::DepthDownSampler>();
    motionVectorPass = AddRenderPipelinePass<Passes::MotionVectorPass>();
    hierarchyZBufferPass = AddRenderPipelinePass<Passes::HierarchyZBufferPass>();
    skyboxPass = AddRenderPipelinePass<SkyboxPass>();
    contactShadowPass = AddRenderPipelinePass<ContactShadowPass>();
    pixelZoomPass = AddRenderPipelinePass<Passes::PixelZoomPass>();
    // s2hDebugPass = AddRenderPipelinePass<Passes::Shader2HumanDebugPass>();

    commandBuffer = GetGfxDriver()->CreateCommandBuffer();
    renderingData.gpuObjectShaderGroups = &gpuObjectShaderGroups;

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
    renderingData.globalResource = perScene.GetGlobalResource();
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
    DrawList deferredDrawList{};
    DrawList forwardDrawList{};
    if (setting->frustumCull)
    {
        auto renderers = scene.GetBVHScene().QueryMeshRenderersInFrustum(renderingData.cameraFrustum);
        std::vector<MeshRenderer*> deferredRenderers;
        std::vector<MeshRenderer*> forwardRenderers;
        deferredRenderers.reserve(renderers.size());
        forwardRenderers.reserve(renderers.size());
        for (auto* renderer : renderers)
        {
            if (renderer->IsForwardRenderer())
                forwardRenderers.push_back(renderer);
            else
                deferredRenderers.push_back(renderer);
        }
        deferredDrawList.Add(deferredRenderers);
        forwardDrawList.Add(forwardRenderers);
    }
    else
    {
        deferredDrawList.Add(renderingScene.GetMeshRenderers());
        forwardDrawList.Add(renderingScene.GetForwardMeshRenderers());
    }

    deferredDrawList.Lock();
    forwardDrawList.Lock();

    ENGINE_BEGIN_PROFILE("Sort");
    deferredDrawList.Sort(camera.GetGameObject()->GetPosition());
    forwardDrawList.Sort(camera.GetGameObject()->GetPosition());
    ENGINE_END_PROFILE; // Sort

    ENGINE_END_PROFILE; // Bulid Scene Draw List

    // Build GPU-driven indirect draw data
    BuildGPUObjectDrawData(*cmd, renderingScene);

    ENGINE_END_PROFILE; // RenderPipeline - Setup

    // Reflection Probe Updateo

    // auto reflectionProbes = renderingScene.GetRenderingObjects<ReflectionProbe>();
    // for (RenderingObjectBase* r : reflectionProbes)
    // {
    //     ReflectionProbe* reflectionProbe = static_cast<ReflectionProbe*>(r);
    // }

    reflectionProbeUpdate->Execute(*cmd, renderingData);
    renderingData.specularCubemap = reflectionProbeUpdate->GetIBLCubemap();

    cmd->BindResource(0, perScene.GetGlobalResource());

    // Shadow Pass
    ENGINE_BEGIN_PROFILE("Shadow")
    shadowRenderer->Execute(*cmd, renderingData);
    ENGINE_END_PROFILE; // Shadow

    // Point Light Shadow Pass
    if (renderingData.pointLightShadowIndex >= 0)
    {
        auto pointLight = renderingData.lights[renderingData.pointLightShadowIndex];
        pointLightShadowRenderer->Setup(*pointLight, renderingData);
        pointLightShadowRenderer->Execute(*cmd, renderingData);
    }

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
        // stencil to 0 (cleared with depth)
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

        // Stencil override: GBuffer objects write stencil bit 1
        Gfx::PipelineConfig::PipelineConfig_t::Stencil gbufferStencil;
        gbufferStencil.testEnable = true;
        gbufferStencil.front.passOp = Gfx::StencilOp::Replace;
        gbufferStencil.front.compareOp = Gfx::CompareOp::Always;
        gbufferStencil.front.compareMask = 0xFF;
        gbufferStencil.front.writeMask = 0xFF;
        gbufferStencil.front.reference = 1;
        gbufferStencil.back = gbufferStencil.front;
        auto stencilOverride = std::optional<Gfx::PipelineConfig::PipelineConfig_t::Stencil>(gbufferStencil);

        // GPU-driven indirect draw for GPU objects
        cmd->BindIndexBuffer(GPUDrivenManager::Instance().GetGlobalBuffer(), 0, Gfx::IndexBufferType::UInt32);
        DrawGPUObjects(*cmd, polygonMode, stencilOverride);

        // Fallback: normal draw for non-GPU objects
        deferredDrawList.DrawRangeHelper(*cmd, 0, deferredDrawList.alphaTestIndex, polygonMode, stencilOverride);
        deferredDrawList.DrawRangeHelper(*cmd, deferredDrawList.alphaTestIndex, deferredDrawList.transparentIndex, polygonMode, stencilOverride);

        // Grass surfaces write stencil bit 2
        cmd->BindResource(0, perScene.GetGlobalResource());
        for (auto* grassSurface : scene.GetRenderingScene().GetGrassSurfaces())
        {
            grassSurfaceRenderer->Draw(*grassSurface, *cmd, renderingData);
        }

        cmd->EndRenderPass();
    }
    cmd->EndLabel(); // GBuffer

    motionVectorPass->Execute(
        *cmd,
        mainDepth,
        mainDepthDescription,
        renderingData,
        renderingData.gpuDrivenIndirectBuffer,
        gpuObjectShaderGroups,
        dynamicMotionDataOffset,
        grassSurfaceRenderer.get(),
        scene.GetRenderingScene().GetGrassSurfaces()
    );
    hierarchyZBufferPass->Execute(*cmd, mainDepth, mainDepthDescription, renderingData);

    auto downSampledDepthCopyDesc = mainDepthDescription;
    downSampledDepthCopyDesc.SetFormat(Gfx::GfxFormat::R32_SFloat);
    downSampledDepthCopyDesc.SetWidth(mainRTSize.x / 2);
    downSampledDepthCopyDesc.SetHeight(mainRTSize.y / 2);
    downSampledDepthCopyDesc.SetRandomWrite(true);
    cmd->AllocateAttachment(downSampledDepthCopy, downSampledDepthCopyDesc);
    depthDownSamplerPass->Setup(mainDepth, downSampledDepthCopy, downSampledDepthCopyDesc);
    depthDownSamplerPass->Execute(*cmd);

    cmd->BindResource(0, perScene.GetGlobalResource());

    Gfx::ImageIdentifier ambientOcclusion = *EngineInternalResources::GetWhiteTexture().GetGfxImage();
    if (setting->useSSIL)
    {
        ssaoPass->ResetDebugState();
        if (setting->ssil.enabled)
        {
            ssilPass->Execute(
                cmd,
                mainColor,
                hierarchyZBufferPass->GetOutputId(),
                albedoGBuffer,
                motionVectorPass->GetOutputId(),
                mainColor,
                setting.Get(),
                renderingData
            );
            ambientOcclusion = ssilPass->GetOutputId();
        }
        else
        {
            ssilPass->ResetDebugState();
        }
    }
    else
    {
        ssilPass->ResetDebugState();
        if (setting->ssao.enabled)
        {
            ssaoPass->Execute(cmd, hierarchyZBufferPass->GetOutputId(), downSampledDepthCopy, mainDepth, mainDepthDescription, setting, renderingData);
            ambientOcclusion = ssaoPass->GetSSAOTex();
        }
        else
        {
            ssaoPass->ResetDebugState();
        }
    }

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
        cmd->BindResource(0, perScene.GetGlobalResource());
        // Upload GPU Parameter
        {
            int plShadowIdx = -1;
            float plFarPlane = 0.0f;
            float plDepthBias = 0.0f;
            glm::vec3 plLightPos = {0, 0, 0};

            if (renderingData.pointLightShadowIndex >= 0)
            {
                plShadowIdx = renderingData.pointLightShadowIndex;
                plFarPlane = pointLightShadowRenderer->GetFarPlane();
                plDepthBias = pointLightShadowRenderer->GetDepthBias();
                plLightPos = pointLightShadowRenderer->GetLightPosition();
            }

            shadingPass->UploadGPUParameter(
                shadowRenderer->GetShadowMapTexelSize(),
                setting->shadowMap.constantBias / 1000.0f,
                setting->shadowMap.normalBias,
                plShadowIdx,
                plFarPlane,
                plDepthBias,
                plLightPos
            );
        }

        cmd->Blit(mainDepth, depthCopy);

        Gfx::ClearValue lightingPassClearValues[] = {{0, 0, 0, 0}, {1, 0}};
        auto diffuseCube = &reflectionProbeUpdate->GetIBLCubemap()->GetImageView(Gfx::ImageViewOption{5, 1, 0, 6, Gfx::ImageAspect::Color, Gfx::ImageViewOption::Type::Cubemap});
        auto specularCube = &reflectionProbeUpdate->GetIBLCubemap()->GetDefaultImageView();

        auto depthImage = GetGfxDriver()->GetImageFromRenderGraph(depthCopy);
        auto& depthImageView = depthImage->GetImageView({Gfx::ImageAspect::Depth});

        Gfx::RenderAttachment lightingPassAttachments[] = {
            {mainColor, Gfx::AttachmentLoadOperation::Load},
            {mainDepth, Gfx::AttachmentLoadOperation::Load, Gfx::AttachmentStoreOperation::Store, Gfx::AttachmentLoadOperation::Load}
        };
        cmd->BeginRenderPass(lightingPassAttachments, lightingPassClearValues);
        shadingPass->Execute(
            *cmd,
            albedoGBuffer,
            normalGBuffer,
            maskGBuffer,
            &depthImageView,
            &shadowRenderer->GetShadowMap()->GetDefaultImageView(),
            &ambientOcclusion,
            &contactShadowPass->GetOutputId(),
            diffuseCube,
            specularCube,
            pointLightShadowRenderer->GetShadowCubemapView(),
            renderingData
        );

        shadingPass->ExecuteGrassLighting(
            *cmd,
            albedoGBuffer,
            normalGBuffer,
            &depthImageView,
            &shadowRenderer->GetShadowMap()->GetDefaultImageView(),
            ambientOcclusion
        );

        cmd->EndRenderPass();
    }
    cmd->EndLabel(); // Shading

    // GI passes
    const Gfx::ImageIdentifier* rtgiOutput = nullptr;
    const Gfx::ImageIdentifier* giIrradiance = nullptr;

    if (setting->rtgi.enabled)
    {
        rtgiPass->Execute(
            cmd,
            hierarchyZBufferPass->GetOutputId(),
            albedoGBuffer,
            normalGBuffer,
            motionVectorPass->GetOutputId(),
            setting.Get(),
            renderingData,
            scene.GetRenderingScene().GetRayTracingSceneHandle(),
            scene.GetRenderingScene().GetRayTracingContext()
        );
        rtgiOutput = &rtgiPass->GetOutputId();
    }

    if (setting->gi.enabled)
    {
        giPass->Execute(
            cmd,
            hierarchyZBufferPass->GetOutputId(),
            albedoGBuffer,
            normalGBuffer,
            motionVectorPass->GetOutputId(),
            setting.Get(),
            renderingData,
            scene.GetRenderingScene().GetRayTracingSceneHandle(),
            scene.GetRenderingScene().GetRayTracingContext()
        );
        giIrradiance = &giPass->GetGIOutput();
    }

    if (rtgiOutput || giIrradiance)
    {
        lightingCombinePass->Execute(cmd, rtgiOutput, giIrradiance, ambientOcclusion, albedoGBuffer, mainColor, renderingData);
    }

    // Forward Pass
    cmd->BeginLabel("Forward", &labelColors.passColor[0]);
    {

        Gfx::RenderAttachment forwardPassAttachments[] = {
            {mainColor, Gfx::AttachmentLoadOperation::Load},
            {mainDepth, Gfx::AttachmentLoadOperation::Load}
        };
        Gfx::ClearValue clears[] = {{0, 0, 0, 0}, {0, 0}};
        if (renderConfig.drawGraphics)
        {
            Graphics::GetSingleton().PrepareDraws(*cmd);
        }
        cmd->BeginRenderPass(forwardPassAttachments, clears);
        cmd->BindResource(0, perScene.GetGlobalResource());

        ExecuteRenderEvents(*cmd, scene, RenderEvents::ForwardOpaque);

        std::optional<Gfx::PolygonMode> forwardPolygonMode = setting->debugDraw.wireframe ? std::optional<Gfx::PolygonMode>(Gfx::PolygonMode::Line) : std::nullopt;
        forwardDrawList.DrawRangeHelper(*cmd, 0, forwardDrawList.alphaTestIndex, forwardPolygonMode, std::nullopt, nullptr, true);
        forwardDrawList.DrawRangeHelper(*cmd, forwardDrawList.alphaTestIndex, forwardDrawList.transparentIndex, forwardPolygonMode, std::nullopt, nullptr, true);

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

        cmd->Blit(mainColor, colorCopy);
        cmd->Blit(mainDepth, depthCopy);

        std::vector<Gfx::DynamicBinding> passBindings = {
            {"colorCopy", colorCopy},
            {"depthCopy", depthCopy}
        };
        // draw objects
        cmd->BeginLabel("Forward Objects", {0.12, 0.64, 0.342, 1.0f});
        cmd->BeginRenderPass(forwardPassAttachments, clears);
        forwardDrawList.DrawRangeHelper(*cmd, forwardDrawList.transparentIndex, forwardDrawList.size(), forwardPolygonMode, std::nullopt, &passBindings, true);

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

    // S2H Debug Merge (overlays GI debug drawings onto mainColor using depth test)
    // if (setting->gi.enabled)
    // {
    //     s2hDebugPass->Execute(
    //         cmd,
    //         giPass->GetS2HDebugOutput(),
    //         hierarchyZBufferPass->GetOutputId(),
    //         mainColor,
    //         {(int)mainRTSize.x, (int)mainRTSize.y},
    //         renderingData
    //     );
    // }

    Gfx::ImageIdentifier postProcessColor = mainColor;
    if (setting->antiAliasing == RenderPipelineSetting::AntiAliasingMode::TAA)
    {
        taaPass->Execute(
            *cmd,
            mainColor,
            hierarchyZBufferPass->GetOutputId(),
            motionVectorPass->GetOutputId(),
            mainColorDescription,
            setting->taa,
            renderingData
        );
        postProcessColor = taaPass->GetOutputId();
    }
    else
    {
        taaPass->ResetHistory();
    }

    if (setting->postProcess.bloom.enabled)
    {
        bloomPass->Execute(*cmd, postProcessColor, mainColorDescription, setting->postProcess.bloom, renderingData);
    }

    // start post procesing
    finalColor = postProcessColor;

    if (setting->postProcess.colorGrading)
    {
        cmd->BeginLabel("Color Grading", &labelColors.passColor[0]);
        colorGradingPass->Execute(
            *cmd,
            GetGfxDriver()->GetImageFromRenderGraph(finalColor),
            mainRTSize,
            setting->postProcess,
            renderingData
        );
        finalColor = colorGradingPass->GetOutputId();
        cmd->EndLabel(); // Color Grading
    }

    // FXAA
    if (setting->antiAliasing == RenderPipelineSetting::AntiAliasingMode::FXAA)
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

    std::optional<Gfx::ImageIdentifier> gbufferDebugImage;
    if (setting->debugDraw.gbufferAlbedo)
    {
        gbufferDebugImage = albedoGBuffer;
    }
    else if (setting->debugDraw.gbufferNormal)
    {
        gbufferDebugImage = normalGBuffer;
    }
    else if (setting->debugDraw.gbufferMask)
    {
        gbufferDebugImage = maskGBuffer;
    }

    if (gbufferDebugImage.has_value())
    {
        finalColor = gbufferDebugImage.value();
    }
    else
    {
        Gfx::ImageIdentifier debugImage;
        for (auto& pass : renderPipelinePasses)
        {
            if (pass->DebugBlit(debugImage))
            {
                finalColor = debugImage;
                break;
            }
        }
    }

    // Pixel Zoom
    if (renderConfig.enablePixelZoom)
    {
        cmd->BeginLabel("PixelZoom", &labelColors.passColor[0]);
        {
            Gfx::RenderImageDescriptor resultDesc(mainRTSize.x, mainRTSize.y, Gfx::GfxFormat::R8G8B8A8_SRGB);
            cmd->AllocateAttachment(pixelZoomPass->GetOutputId(), resultDesc);
            pixelZoomPass->Execute(
                *cmd,
                finalColor,
                pixelZoomPass->GetOutputId(),
                renderConfig.pixelZoomMousePos,
                glm::vec2(mainRTSize.x, mainRTSize.y)
            );
            finalColor = pixelZoomPass->GetOutputId();
        }
        cmd->EndLabel(); // PixelZoom
    }

    glm::mat4 renderedViewProjection = perScene.cameraParameter.viewProjection;
    glm::mat4 renderedInverseViewProjection = perScene.cameraParameter.invNDCToWorld;
    if (setting->antiAliasing == RenderPipelineSetting::AntiAliasingMode::TAA)
    {
        perScene.cameraParameter = perScene.unjitteredCameraParameter;
        cmd->UploadData(*perScene.camera, (uint8_t*)&perScene.cameraParameter, sizeof(GPUParameter::Camera));
    }

    cmd->EndLabel(); // Render Scene

    if (!IsCommandBufferOverriden())
    {
        GetGfxDriver()->ExecuteCommandBuffer(*cmd);
        cmd->Reset(true);
    }

    // Update camera temporal state for the next frame
    camera.SetPreviousViewProjection(renderedViewProjection);
    camera.SetInvPreviousViewProjection(renderedInverseViewProjection);
    previousTemporalJitterUv = currentTemporalJitterUv;
} // namespace Rendering

PerScene::PerScene()
{
    auto& gpuDriven = GPUDrivenManager::Instance();
    scene = gpuDriven.GetSceneBuffer();
    camera = gpuDriven.GetCameraBuffer();
    mainLightShadow = gpuDriven.GetMainLightShadowBuffer();
}

Gfx::ShaderResource* PerScene::GetGlobalResource() const
{
    return GPUDrivenManager::Instance().GetGlobalDescriptorSet();
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

    cmd->BindResource(0, perScene.GetGlobalResource());

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
    UpdateSceneInfo(cmd, scene, camera, screenSize);
    return true;
}

void RenderPipeline::UpdateSceneInfo(Gfx::CommandBuffer* cmd, Scene& scene, Camera& camera, float2 screenSize)
{
    auto& cameraParam = perScene.cameraParameter;
    auto& sceneParam = perScene.sceneParameter;
    auto& mainLightShadowParam = perScene.mainLightShadowParameter;
    auto& unjitteredCameraParam = perScene.unjitteredCameraParameter;
    unjitteredCameraParam = RenderingUtils::CreateCameraGPUParameter(camera, screenSize);

    // populate previous matrices from camera component
    unjitteredCameraParam.previousViewProjection = camera.GetPreviousViewProjection();
    unjitteredCameraParam.invPreviousViewProjection = camera.GetInvPreviousViewProjection();
    unjitteredCameraParam.temporalJitter = glm::vec4(0.0f);
    cameraParam = unjitteredCameraParam;

    if (temporalJitterCamera != &camera)
    {
        temporalJitterCamera = &camera;
        previousTemporalJitterUv = glm::vec2(0.0f);
    }

    currentTemporalJitterUv = glm::vec2(0.0f);
    if (setting->antiAliasing == RenderPipelineSetting::AntiAliasingMode::TAA)
    {
        glm::vec2 jitter = TAAPass::GetProjectionJitterNdc(frameIndex, screenSize, setting->taa.jitterScale);
        currentTemporalJitterUv = jitter * 0.5f;
        glm::mat4 jitterTransform(1.0f);
        jitterTransform[3][0] = jitter.x;
        jitterTransform[3][1] = jitter.y;
        cameraParam.projection = jitterTransform * cameraParam.projection;
        cameraParam.viewProjection = cameraParam.projection * cameraParam.view;
        cameraParam.invProjection = glm::inverse(cameraParam.projection);
        cameraParam.invNDCToWorld = cameraParam.invView * cameraParam.invProjection;
    }
    cameraParam.temporalJitter = glm::vec4(
        currentTemporalJitterUv,
        previousTemporalJitterUv
    );

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
    sceneParam.previousTime = sceneParam.time - Time::DeltaTime();
    sceneParam.frameIndex = static_cast<float>(frameIndex++);
    {
        Light* mainLight = nullptr;
        ENGINE_BEGIN_PROFILE("Get Active Lights")
        renderingData.lights = scene.GetActiveLights();
        renderingData.mainLightIndex = -1;
        renderingData.pointLightShadowIndex = -1;
        ENGINE_END_PROFILE
        auto& lights = renderingData.lights;

        sceneParam.lightCount = lights.size();
        for (int i = 0; i < lights.size(); ++i)
        {
            sceneParam.lights[i].ambientScale = lights[i]->GetAmbientScale();
            sceneParam.lights[i].lightColor = glm::vec4(lights[i]->GetLinearLightColor(), 1.0);
            sceneParam.lights[i].intensity = lights[i]->GetIntensity();
            sceneParam.lights[i].skyboxIntensity = lights[i]->GetSkyboxIntensity();
            sceneParam.lights[i].skyColor = glm::vec4(lights[i]->GetSkyColor(), 1.0);
            sceneParam.lights[i].skyHorizonFalloffColor = glm::vec4(lights[i]->GetSkyHorizonFalloffColor(), 1.0);
            sceneParam.lights[i].skyHorizonColor = glm::vec4(lights[i]->GetSkyHorizonColor(), 1.0);
            sceneParam.lights[i].skySunColor = glm::vec4(lights[i]->GetSkySunColor(), 1.0);
            sceneParam.lights[i].skySunCoreColor = glm::vec4(lights[i]->GetSkySunCoreColor(), 1.0);
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
                        sceneParam.lights[i].range = lights[i]->GetRange();
                        sceneParam.lights[i].pointLightTerm1 = lights[i]->GetPointLightLinear();
                        sceneParam.lights[i].pointLightTerm2 = lights[i]->GetPointLightDistance();

                        // track first point light with shadow enabled
                        if (renderingData.pointLightShadowIndex < 0 && lights[i]->IsPointLightShadowEnabled())
                        {
                            renderingData.pointLightShadowIndex = i;
                        }
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

    cmd->UploadData(*perScene.camera, (uint8_t*)&cameraParam, sizeof(GPUParameter::Camera));
    cmd->UploadData(*perScene.scene, (uint8_t*)&sceneParam, sizeof(GPUParameter::Scene));
    cmd->UploadData(
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
    finalColorId = finalColor;

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

void RenderPipeline::SetupGPUDrivenBindings()
{
}

void RenderPipeline::BuildGPUObjectDrawData(Gfx::CommandBuffer& cmd, RenderingScene& renderingScene)
{
    gpuObjectShaderGroups.clear();
    gpuObjectOffsets.clear();
    flatDrawInfos.clear();
    allIndirectCmds.clear();
    allIndirectCmdsExtra.clear();
    dynamicMotionDatas.clear();
    dynamicMotionDataOffset = InvalidTextureIndex;
    renderingData.gpuDrivenIndirectDrawCount = 0;
    renderingData.gpuDrivenIndirectBuffer = nullptr;

    auto gpuRenderers = renderingScene.GetGPUObjectRenderers();
    if (gpuRenderers.empty())
        return;

    // 1. Gather all draw commands into a flat array using direct material indexing
    for (auto* renderer : gpuRenderers)
    {
        if (!renderer || !renderer->IsActiveInScene() || renderer->IsForwardRenderer())
            continue;

        uint32_t previousSkeletonOffset = renderer->UploadGPUDrivenFrameData(cmd);

        const auto& gpuObjectDescriptor = renderer->GetGpuObjectDescriptor();
        const auto& gpuRenderDataListDescriptor = renderer->GetGpuRenderDataListDescriptor();
        MeshRenderer::MotionState* motionState = renderer->FlushMotionState();

        for (size_t renderDataListIndex = 0; renderDataListIndex < gpuRenderDataListDescriptor.renderDataList.size(); ++renderDataListIndex)
        {
            auto mat = renderer->GetGpuRenderMaterial(static_cast<int>(renderDataListIndex));
            if (mat)
            {
                auto geometryDescriptor = renderer->GetGpuGeometry(static_cast<int>(renderDataListIndex));
                auto& config = mat->GetShaderConfig();
                flatDrawInfos.push_back({mat->GetShaderProgram(), &config, motionState, config.GetHash(), geometryDescriptor.geometry.indexCount, static_cast<uint32_t>(geometryDescriptor.geometry.indexOffset / sizeof(uint32_t)), static_cast<uint32_t>(renderDataListIndex), static_cast<uint32_t>(gpuObjectDescriptor.dataAlloc.offset), previousSkeletonOffset, renderer->CastsShadows()});
            }
        }
    }

    if (flatDrawInfos.empty())
        return;

    // 2. Sort the flat array by ShaderProgram pointer to group them together
    std::sort(flatDrawInfos.begin(), flatDrawInfos.end(), [](const FlatDrawInfo& a, const FlatDrawInfo& b)
              { 
                // prioritize motion objects
                bool aHasMotion = a.motionState != nullptr;
                bool bHasMotion = b.motionState != nullptr;
                return std::tie(aHasMotion, a.shaderProgram, a.pipelineConfigHash, a.castsShadows) < std::tie(bHasMotion, b.shaderProgram, b.pipelineConfigHash, b.castsShadows); });

    // 3. Build the indirect command buffers and shader groups in a single pass
    bool currentHasMotion = false;
    Gfx::ShaderProgram* currentShader = nullptr;
    const Gfx::PipelineConfig* currentConfig = nullptr;
    size_t currentConfigHash = 0;
    uint32_t currentGroupStart = 0;
    uint32_t currentDynamicMotionDataStart = 0;
    bool currentCastsShadows = true;

    // dispatching each draw into their group in a sequential stable way
    // dynamicMotionDatas are accessed by positional corespondence, so be careful
    for (size_t i = 0; i < flatDrawInfos.size(); ++i)
    {
        const auto& info = flatDrawInfos[i];

        bool infoHasMotion = info.motionState != nullptr;
        if (info.shaderProgram != currentShader || info.pipelineConfigHash != currentConfigHash || infoHasMotion != currentHasMotion || info.castsShadows != currentCastsShadows)
        {
            if (currentShader != nullptr && currentConfig != nullptr)
            {
                gpuObjectShaderGroups.push_back({currentShader, currentConfig, currentGroupStart, currentDynamicMotionDataStart, static_cast<uint32_t>(i - currentGroupStart), currentHasMotion, currentCastsShadows});
            }

            currentHasMotion = infoHasMotion;
            currentShader = info.shaderProgram;
            currentConfig = info.pipelineConfig;
            currentConfigHash = currentConfig != nullptr ? currentConfig->GetHash() : 0;
            currentGroupStart = static_cast<uint32_t>(i);
            currentDynamicMotionDataStart = static_cast<uint32_t>(dynamicMotionDatas.size());
            currentCastsShadows = info.castsShadows;
        }

        if (infoHasMotion)
        {
            dynamicMotionDatas.push_back({
                .previousWorldMatrix = info.motionState->previousFrameWorldMatrix,
                .previousSkeletonOffset = info.previousSkeletonOffset,
            });
        }

        allIndirectCmds.push_back({.indexCount = info.indexCount, .instanceCount = 1, .firstIndex = info.firstIndex, .vertexOffset = 0, .firstInstance = 0});
        allIndirectCmdsExtra.push_back({.objectOffset = info.objectOffset, .renderDataIndex = info.firstInstance});
    }

    // Push the final group
    if (currentShader != nullptr)
    {
        gpuObjectShaderGroups.push_back({currentShader, currentConfig, currentGroupStart, currentDynamicMotionDataStart, static_cast<uint32_t>(flatDrawInfos.size() - currentGroupStart), currentHasMotion, currentCastsShadows});
    }

    auto& gpuDriven = GPUDrivenManager::Instance();
    if (!dynamicMotionDatas.empty())
    {
        auto dynamicMotionDataAllocation = gpuDriven.UploadDynamicData(
            cmd,
            dynamicMotionDatas.data(),
            static_cast<uint32_t>(dynamicMotionDatas.size() * sizeof(GPUDynamicMotionData)),
            16
        );
        if (dynamicMotionDataAllocation.IsValid())
            dynamicMotionDataOffset = dynamicMotionDataAllocation.offset;
    }

    for (auto* renderer : gpuRenderers)
    {
        if (renderer && renderer->IsActiveInScene() && !renderer->IsForwardRenderer())
            renderer->CommitMotionState();
    }

    IndirectDrawData indirectDrawData = gpuDriven.UploadIndirectDrawData(cmd, allIndirectCmds, allIndirectCmdsExtra);
    if (indirectDrawData.commandBuffer == nullptr)
    {
        gpuObjectShaderGroups.clear();
        return;
    }

    for (auto& group : gpuObjectShaderGroups)
    {
        group.firstDrawIndex += indirectDrawData.firstDrawIndex;
    }

    renderingData.gpuDrivenIndirectBuffer = indirectDrawData.commandBuffer;
    renderingData.gpuDrivenIndirectDrawCount = indirectDrawData.drawCount;
}

void RenderPipeline::DrawGPUObjects(Gfx::CommandBuffer& cmd, std::optional<Gfx::PolygonMode> polygonModeOverride, std::optional<Gfx::PipelineConfig::PipelineConfig_t::Stencil> stencilOverride)
{
    auto* indirectCommandBuffer = renderingData.gpuDrivenIndirectBuffer;
    if (gpuObjectShaderGroups.empty() || !indirectCommandBuffer)
        return;

    for (auto& group : gpuObjectShaderGroups)
    {
        bool configModified = polygonModeOverride.has_value() || stencilOverride.has_value();
        if (configModified)
        {
            auto modifiedConfig = **group.pipelineConfig;
            if (polygonModeOverride.has_value())
                modifiedConfig.polygonMode = polygonModeOverride.value();
            if (stencilOverride.has_value())
                modifiedConfig.stencil = stencilOverride.value();
            cmd.BindShaderProgram(group.shaderProgram, modifiedConfig);
        }
        else
        {
            cmd.BindShaderProgram(group.shaderProgram, *group.pipelineConfig);
        }

        struct Data
        {
            float4x4 d0 = {};
            float4x4 d1 = {};
        } pconst;
        uint32_t firstDrawIndex = group.firstDrawIndex;
        pconst.d0[0][0] = std::bit_cast<float>(firstDrawIndex);
        cmd.SetPushConstant(group.shaderProgram, &pconst);
        cmd.DrawIndexedIndirect(
            indirectCommandBuffer,
            group.firstDrawIndex * sizeof(DrawIndexedIndirectCommand),
            group.drawCount,
            sizeof(DrawIndexedIndirectCommand)
        );
    }
}

} // namespace Rendering
  //
