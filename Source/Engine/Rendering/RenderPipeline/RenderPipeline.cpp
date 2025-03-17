#include "RenderPipeline.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/EngineInternalResources.hpp"
#include "Core/Scene/Scene.hpp"
#include "Core/Texture.hpp"
#include "Core/Time.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Profiler/Profiler.hpp"
#include "Rendering/RenderingUtils.hpp"
#include "Rendering/ShaderLibrary.hpp"

namespace Rendering
{

RenderPipeline::RenderPipeline()
{
    commandBuffer = GetGfxDriver()->CreateCommandBuffer();

    Gfx::ImageDescription interleavedGradientNoiseDesc(32, 32, 1, Gfx::GfxFormat::R8_UNorm);
    renderingData.interleavedGradientNoise =
        GetGfxDriver()->CreateImage(interleavedGradientNoiseDesc, Gfx::ImageUsage::Texture | Gfx::ImageUsage::Storage);

    // generate interleavedGradientNoise
    auto interleavedGradientNoiseShader = ShaderLibrary::GetShader(ShaderLibrary::InterleavedGradientNoise);
    interleavedGradientNoiseMat.SetShader(interleavedGradientNoiseShader);
    interleavedGradientNoiseMat.SetTexture("tex", renderingData.interleavedGradientNoise.get());
    auto cmd = GetGfxDriver()->CreateCommandBuffer();
    cmd->BindResource(0, interleavedGradientNoiseMat.GetShaderResource());
    cmd->BindShaderProgram(
        interleavedGradientNoiseShader->GetShaderProgram(),
        interleavedGradientNoiseShader->GetShaderProgram()->GetDefaultShaderConfig()
    );
    cmd->Dispatch((interleavedGradientNoiseDesc.width + 7) / 8, (interleavedGradientNoiseDesc.height + 7) / 8, 1);
    GetGfxDriver()->ExecuteCommandBuffer(*cmd);
}

void RenderPipeline::Render(Scene& scene, Camera& camera, glm::float2 screenSize)
{
    ENGINE_BEGIN_PROFILE("RenderPipeline - Setup")
    setting = scene.GetRenderPipelineSetting();
    Gfx::CommandBuffer* cmd = commandBuffer.get();

    UpdateSceneInfo(scene, camera, screenSize);
    FrameSetup(cmd, scene, camera, screenSize);

    // Setup
    glm::float2 mainRTSize = {mainColorDescription.GetWidth(), mainColorDescription.GetHeight()};

    DrawList sceneDrawList;
    SceneRendererSorter()(scene, camera, sceneDrawList);
    ENGINE_END_PROFILE

    cmd->BindResource(0, perScene.gpuResourceSet.get());

    // Shadow Pass
    cmd->BeginLabel("Shadow Map", &labelColors.passColor[0]);
    {
        if (shadowMapPass.updateMainLightShadow)
        {
            Gfx::ClearValue shadowMapClears[] = {{1.0f, 0}};
            cmd->BeginRenderPass(shadowMapPass.pass, shadowMapClears);
            auto program = shadowMapPass.shadowMapShader->GetShaderProgram();
            auto programSkinned = shadowMapPass.shadowMapShaderSkinned->GetShaderProgram();

            for (auto& draw : sceneDrawList)
            {
                auto programUsed = program;
                [[unlikely]]
                if (draw.skinned)
                {
                    programUsed = programSkinned;
                    if (draw.objectResource)
                        cmd->BindResource(1, draw.objectResource);
                }
                else
                {
                    auto ps = draw.GetPushConstant();
                    cmd->SetPushConstant(programUsed, (void*)&ps);
                }
                cmd->BindShaderProgram(programUsed, programUsed->GetDefaultShaderConfig());

                cmd->BindVertexBuffer(draw.vertexBufferBinding, 0);
                cmd->BindIndexBuffer(draw.indexBuffer, 0, draw.indexBufferType);
                cmd->DrawIndexed(draw.indexCount, 1, 0, 0, 0);
            }

            cmd->EndRenderPass();
        }
    }
    cmd->EndLabel();

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
                .shadowMapTexelSize = shadowMapPass.shadowMapTexelSize,
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
        shadingPass.gpuResource->SetImage("shadowMap"_shaderBinding, shadowMapPass.shadowMap.get());
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

        cmd->BeginLabel("Draw Graphics", &labelColors.passColor[0]);
        RenderingUtils::DrawGraphics(*cmd);
        cmd->EndLabel();

        // skybox
        cmd->BeginLabel("Skybox", &labelColors.passColor1[0]);
        cmd->BindVertexBuffer(skyboxPass.cube->GetGfxVertexBufferBindings(), 0);
        cmd->BindIndexBuffer(skyboxPass.cube->GetIndexBuffer(), 0, skyboxPass.cube->GetIndexBufferType());
        cmd->BindShaderProgram(
            skyboxPass.skyboxShader->GetShaderProgram(),
            skyboxPass.skyboxShader->GetShaderProgram()->GetDefaultShaderConfig()
        );
        cmd->DrawIndexed(skyboxPass.cube->GetIndexCount(), 1, 0, 0, 0);
        cmd->EndLabel();

        cmd->EndRenderPass();

        scene.GetRenderingScene().Execute(RenderingEvent::Skybox, *cmd, renderingData);

        // draw objects
        cmd->BeginRenderPass(forwardPass.pass, clears);
        sceneDrawList.DrawRangeHelper(*cmd, sceneDrawList.transparentIndex, sceneDrawList.size());
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

    // Debug
    {}

    GetGfxDriver()->ExecuteCommandBuffer(*cmd);

    cmd->Reset(true);
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

RenderPipeline::ShadowMapPass::ShadowMapPass()
{
    pass = Gfx::RG::RenderPass(1, 1);
    pass.SetSubpass(
        0,
        {},
        Gfx::RG::SubpassAttachment{0, Gfx::AttachmentLoadOperation::Clear, Gfx::AttachmentStoreOperation::Store}
    );
    pass.SetName("ShadowMap pass");
    shadowMapShader = ShaderLibrary::GetShader(ShaderLibrary::ShadowMapObject);
    shadowMapShaderSkinned = ShaderLibrary::GetShader(ShaderLibrary::ShadowMapObjectSkinned);

    shadowDescription = Gfx::ImageDescription(shadowMapTexelSize.z, shadowMapTexelSize.w, Gfx::GfxFormat::D32_SFloat);

    shadowMap = GetGfxDriver()->CreateImage(
        shadowDescription,
        Gfx::ImageUsage::DepthStencilAttachment | Gfx::ImageUsage::Texture
    );
    shadowMapId = *shadowMap;

    pass.SetAttachment(0, shadowMapId);
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

RenderPipeline::SkyboxPass::SkyboxPass()
{
    pass = Gfx::RG::RenderPass::Default(
        "Skybox",
        Gfx::AttachmentLoadOperation::Load,
        Gfx::AttachmentStoreOperation::Store,
        Gfx::AttachmentLoadOperation::Load,
        Gfx::AttachmentStoreOperation::Store
    );

    cube = EngineInternalResources::GetCubeMesh();
    skyboxShader = ShaderLibrary::GetShader(ShaderLibrary::Skybox);
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
    cmd->BindResource(mat.GetSet("perMaterial"), mat.GetShaderResource());
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

void RenderPipeline::RenderSkyboxOnly(Scene& scene, Camera& camera)
{
    auto cmd = commandBuffer.get();
    cmd->BeginLabel("Skybox", &labelColors.passColor1[0]);
    cmd->BindVertexBuffer(skyboxPass.cube->GetGfxVertexBufferBindings(), 0);
    cmd->BindIndexBuffer(skyboxPass.cube->GetIndexBuffer(), 0, skyboxPass.cube->GetIndexBufferType());
    cmd->BindShaderProgram(
        skyboxPass.skyboxShader->GetShaderProgram(),
        skyboxPass.skyboxShader->GetShaderProgram()->GetDefaultShaderConfig()
    );
    cmd->DrawIndexed(skyboxPass.cube->GetIndexCount(), 1, 0, 0, 0);
    cmd->EndLabel();

    GetGfxDriver()->ExecuteCommandBuffer(*cmd);
    cmd->Reset(true);
}

void RenderPipeline::FrameSetup(Gfx::CommandBuffer* cmd, Scene& scene, Camera& camera, float2 screenSize)
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
        return;
    }

    UpdateSceneInfo(scene, camera, screenSize);
    renderingData.sceneInfo = &perScene.cpuParameter;
    renderingData.mainCamera = &camera;
    renderingData.mainColor = GetGfxDriver()->GetImageFromRenderGraph(mainColor);
    renderingData.mainDepth = GetGfxDriver()->GetImageFromRenderGraph(mainDepth);
}

void RenderPipeline::UpdateSceneInfo(Scene& scene, Camera& camera, float2 screenSize)
{
    auto camGo = camera.GetGameObject();

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

} // namespace Rendering
  //
