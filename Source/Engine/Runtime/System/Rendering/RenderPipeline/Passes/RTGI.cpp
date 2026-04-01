#include "RTGI.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Runtime/Object/Component/Camera.hpp"
#include "Engine/Runtime/System/Rendering/GPUDriven/GPUDrivenManager.hpp"
#include "Engine/Runtime/System/Rendering/RenderingData.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"

namespace Rendering::Passes
{

RTGI::RTGI()
{
    rtgiShader = ShaderLibrary::GetShader(Shaders::PostProcess_RTGI);
    temporalShader = ShaderLibrary::GetShader(Shaders::PostProcess_RTGI_Temporal);
    variancePrefilterShader = ShaderLibrary::GetShader(Shaders::PostProcess_RTGI_VariancePrefilter);
    atrousShader = ShaderLibrary::GetShader(Shaders::PostProcess_RTGI_ATrous);

    mat.SetShader(rtgiShader);
    temporalMat.SetShader(temporalShader);
}

void RTGI::EnsureHistoryBuffers(int width, int height)
{
    if (historySize.x == width && historySize.y == height)
        return;

    historySize = {width, height};
    historyValid = false;

    Gfx::ImageDescription colorDesc(width, height, Gfx::GfxFormat::R16G16B16A16_SFloat);
    historyColor = GetGfxDriver()->CreateImage(
        colorDesc,
        Gfx::ImageUsage::Texture | Gfx::ImageUsage::Storage | Gfx::ImageUsage::TransferDst | Gfx::ImageUsage::ColorAttachment
    );
    historyColor->SetName("RTGI_HistoryColor");

    Gfx::ImageDescription momentsDesc(width, height, Gfx::GfxFormat::R16G16B16A16_SFloat);
    historyMoments = GetGfxDriver()->CreateImage(
        momentsDesc,
        Gfx::ImageUsage::Texture | Gfx::ImageUsage::Storage | Gfx::ImageUsage::TransferDst | Gfx::ImageUsage::ColorAttachment
    );
    historyMoments->SetName("RTGI_HistoryMoments");

    Gfx::ImageDescription depthHistoryDesc(width, height, Gfx::GfxFormat::R32G32_SFloat);
    historyDepth = GetGfxDriver()->CreateImage(
        depthHistoryDesc,
        Gfx::ImageUsage::Texture | Gfx::ImageUsage::TransferDst | Gfx::ImageUsage::ColorAttachment
    );
    historyDepth->SetName("RTGI_HistoryDepth");

    Gfx::ImageDescription normalHistoryDesc(width, height, Gfx::GfxFormat::A2B10G10R10_UNorm); // Matches G-Buffer normal format
    historyNormal = GetGfxDriver()->CreateImage(
        normalHistoryDesc,
        Gfx::ImageUsage::Texture | Gfx::ImageUsage::TransferDst | Gfx::ImageUsage::ColorAttachment
    );
    historyNormal->SetName("RTGI_HistoryNormal");

    GetGfxDriver()->InitGfxImage(*historyColor, float4(0, 0, 0, 0));
    GetGfxDriver()->InitGfxImage(*historyMoments, float4(0, 0, 0, 0));
    GetGfxDriver()->InitGfxImage(*historyDepth, float4(0, 0, 0, 0));
    GetGfxDriver()->InitGfxImage(*historyNormal, float4(0, 1, 0, 0)); // Default normal pointing up
}

void RTGI::Execute(
    Gfx::CommandBuffer* cmd,
    const Gfx::ImageIdentifier& hizTex,
    const Gfx::ImageIdentifier& albedoTex,
    const Gfx::ImageIdentifier& normalTex,
    const Gfx::ImageIdentifier& motionVectorTex,
    RenderPipelineSetting* setting,
    RenderingData& renderingData,
    Gfx::RayTracingSceneHandle tlas,
    Gfx::RayTracingContext* rtContext
)
{
    if (!setting->rtgi.enabled || tlas == -1 || rtContext == nullptr)
        return;

    cmd->BeginLabel("RTGI", {0.8, 0.4, 0.1, 1.0});

    int width = renderingData.screenSize.x;
    int height = renderingData.screenSize.y;

    EnsureHistoryBuffers(width, height);

    glm::float4 rtSize(width, height, 1.0f / width, 1.0f / height);

    Gfx::RenderImageDescriptor colorDesc(width, height, Gfx::GfxFormat::R16G16B16A16_SFloat);
    colorDesc.SetRandomWrite(true);

    // -------------------------------------------------------------------------
    // Pass 1: Ray tracing — one sample per pixel into rtgiRaw
    // -------------------------------------------------------------------------
    cmd->BeginLabel("RTGI_RT", {0.8, 0.4, 0.1, 1.0});

    cmd->AllocateAttachment(rtgiRaw, colorDesc);

    mat.SetTexture("depthTex", GetGfxDriver()->GetImageFromRenderGraph(hizTex));
    mat.SetTexture("albedoTex", GetGfxDriver()->GetImageFromRenderGraph(albedoTex));
    mat.SetTexture("normalTex", GetGfxDriver()->GetImageFromRenderGraph(normalTex));
    mat.SetTexture("noiseTex", renderingData.blueNoise.GetNoiseTexture());
    mat.SetTexture("environmentMap", renderingData.specularCubemap);
    mat.SetTexture("outRtgiTex", GetGfxDriver()->GetImageFromRenderGraph(rtgiRaw));
    mat.SetVector("rtSize", rtSize);

    debugRTGI = setting->rtgi.debug_rtgiOutput;

    auto* rtProgram = mat.GetShaderProgram();
    mat.GetShaderResource()->SetAccelerationStructure("sceneBVH", 0, rtContext, tlas);

    cmd->BindResource(0, renderingData.globalResource);
    cmd->BindResource(mat.GetSet(Gfx::DescriptorSetSemantics::Material), mat.GetShaderResource());
    cmd->BindShaderProgram(rtProgram, rtProgram->GetDefaultShaderConfig());
    cmd->Dispatch((width + 7) / 8, (height + 7) / 8, 1);

    cmd->EndLabel();

    // -------------------------------------------------------------------------
    // Pass 2: Temporal accumulation — blend with history, accumulate moments
    // -------------------------------------------------------------------------
    cmd->BeginLabel("RTGI_Temporal", {0.6, 0.3, 0.8, 1.0});

    Gfx::RenderImageDescriptor momentsDesc(width, height, Gfx::GfxFormat::R16G16B16A16_SFloat);
    momentsDesc.SetRandomWrite(true);

    cmd->AllocateAttachment(rtgiAccumulated, colorDesc);
    cmd->AllocateAttachment(momentsOut, momentsDesc);

    const auto& svgf = setting->rtgi.svgf;

    temporalMat.SetTexture("rtgiRawTex", GetGfxDriver()->GetImageFromRenderGraph(rtgiRaw));
    temporalMat.SetTexture("historyColorTex", historyColor.get());
    temporalMat.SetTexture("historyMomentsTex", historyMoments.get());
    temporalMat.SetTexture("historyDepthTex", historyDepth.get());
    temporalMat.SetTexture("motionVecTex", GetGfxDriver()->GetImageFromRenderGraph(motionVectorTex));
    temporalMat.SetTexture("depthTex", GetGfxDriver()->GetImageFromRenderGraph(hizTex));
    temporalMat.SetTexture("normalTex", GetGfxDriver()->GetImageFromRenderGraph(normalTex));
    temporalMat.SetTexture("historyNormalTex", historyNormal.get());
    temporalMat.SetTexture("outAccumulated", GetGfxDriver()->GetImageFromRenderGraph(rtgiAccumulated));
    temporalMat.SetTexture("outMoments", GetGfxDriver()->GetImageFromRenderGraph(momentsOut));
    temporalMat.SetVector("rtgiParams", rtSize);
    temporalMat.SetVector("temporalParams", glm::float4(svgf.temporalAlpha, historyValid ? 1.0f : 0.0f, 0.0f, 0.0f));

    auto* temporalProgram = temporalMat.GetShaderProgram();
    cmd->BindResource(0, renderingData.globalResource);
    cmd->BindResource(temporalMat.GetSet(Gfx::DescriptorSetSemantics::Material), temporalMat.GetShaderResource());
    cmd->BindShaderProgram(temporalProgram, temporalProgram->GetDefaultShaderConfig());
    cmd->Dispatch((width + 7) / 8, (height + 7) / 8, 1);

    cmd->EndLabel();

    // -------------------------------------------------------------------------
    // Pass 2.5: Variance pre-filter — 3x3 Gaussian on variance to stabilize it
    // -------------------------------------------------------------------------
    cmd->BeginLabel("RTGI_VariancePrefilter", {0.4, 0.4, 0.8, 1.0});

    cmd->AllocateAttachment(rtgiPrefiltered, colorDesc);

    renderingData.pipelineAllocator->AllocateBuffer(variancePrefilterParamBuffer, sizeof(glm::float4));
    variancePrefilterParamBuffer.Write(&rtSize, sizeof(glm::float4));

    cmd->BindResource(0, {
                             Gfx::DynamicBinding("perMaterial", *variancePrefilterParamBuffer.GetBuffer()),
                             Gfx::DynamicBinding("inTex", *GetGfxDriver()->GetImageFromRenderGraph(rtgiAccumulated)),
                             Gfx::DynamicBinding("outTex", *GetGfxDriver()->GetImageFromRenderGraph(rtgiPrefiltered)),
                         });

    auto* variancePrefilterProgram = variancePrefilterShader->GetShaderProgram();
    cmd->BindShaderProgram(variancePrefilterProgram, variancePrefilterProgram->GetDefaultShaderConfig());
    cmd->Dispatch((width + 7) / 8, (height + 7) / 8, 1);

    cmd->EndLabel();

    // -------------------------------------------------------------------------
    // Pass 3: À-trous wavelet spatial filter (ping-pong, N iterations)
    // -------------------------------------------------------------------------
    cmd->BeginLabel("RTGI_ATrous", {0.3, 0.6, 0.8, 1.0});

    cmd->AllocateAttachment(rtgiPing, colorDesc);
    cmd->AllocateAttachment(rtgiPong, colorDesc);

    auto* atrousProgram = atrousShader->GetShaderProgram();

    struct RTGIATrousParamUBO
    {
        glm::float4 rtgiParams;
        glm::float4 filterParams;
    };

    Gfx::ImageIdentifier* src = &rtgiPrefiltered;
    Gfx::ImageIdentifier* dst = &rtgiPong;

    int iterations = glm::clamp(svgf.atrousIterations, 1, 5);
    for (int i = 0; i < iterations; ++i)
    {
        RTGIATrousParamUBO params;
        params.rtgiParams = rtSize;
        params.filterParams = glm::float4(
            static_cast<float>(1 << i),
            svgf.sigmaDepth,
            svgf.sigmaNormal,
            svgf.sigmaLuminance
        );
        renderingData.pipelineAllocator->AllocateBuffer(atrousPassResources[i].paramBuffer, sizeof(RTGIATrousParamUBO));
        atrousPassResources[i].paramBuffer.Write(&params, sizeof(RTGIATrousParamUBO));

        cmd->BindResource(0, renderingData.globalResource);
        cmd->BindResource(1, {
                                 Gfx::DynamicBinding("perMaterial", *atrousPassResources[i].paramBuffer.GetBuffer()),
                                 Gfx::DynamicBinding("inTex", *GetGfxDriver()->GetImageFromRenderGraph(*src)),
                                 Gfx::DynamicBinding("depthTex", *GetGfxDriver()->GetImageFromRenderGraph(hizTex)),
                                 Gfx::DynamicBinding("normalTex", *GetGfxDriver()->GetImageFromRenderGraph(normalTex)),
                                 Gfx::DynamicBinding("outTex", *GetGfxDriver()->GetImageFromRenderGraph(*dst)),
                             });
        cmd->BindShaderProgram(atrousProgram, atrousProgram->GetDefaultShaderConfig());
        cmd->Dispatch((width + 7) / 8, (height + 7) / 8, 1);

        std::swap(src, dst);
        // After the first iteration dst would point back to rtgiPrefiltered;
        // redirect to rtgiPing to avoid overwriting the source.
        if (dst == &rtgiPrefiltered)
            dst = &rtgiPing;
    }

    cmd->EndLabel();

    // src now holds the identifier for the final filtered result
    rtgi = *src;

    // -------------------------------------------------------------------------
    // Pass 4: Copy final result and moments into persistent history for next frame
    // -------------------------------------------------------------------------
    Gfx::Image* finalImage = GetGfxDriver()->GetImageFromRenderGraph(*src);
    Gfx::Image* momentsImage = GetGfxDriver()->GetImageFromRenderGraph(momentsOut);
    Gfx::Image* hizImage = GetGfxDriver()->GetImageFromRenderGraph(hizTex);
    Gfx::Image* currentNormalImage = GetGfxDriver()->GetImageFromRenderGraph(normalTex);

    cmd->Blit(Gfx::ImageIdentifier(*finalImage), Gfx::ImageIdentifier(*historyColor));
    cmd->Blit(Gfx::ImageIdentifier(*momentsImage), Gfx::ImageIdentifier(*historyMoments));
    cmd->Blit(Gfx::ImageIdentifier(*currentNormalImage), Gfx::ImageIdentifier(*historyNormal));

    // Copy mip 0 of HZB to the single-mip history depth buffer
    Gfx::BlitOp depthBlitOp;
    depthBlitOp.srcMip = 0;
    depthBlitOp.dstMip = 0;
    cmd->Blit(Gfx::ImageIdentifier(*hizImage), Gfx::ImageIdentifier(*historyDepth), depthBlitOp);

    historyValid = true;

    cmd->EndLabel(); // RTGI
}

bool RTGI::DebugBlit(Gfx::ImageIdentifier& dst)
{
    if (debugRTGI)
    {
        dst = rtgi;
        return true;
    }
    return false;
}

} // namespace Rendering::Passes
