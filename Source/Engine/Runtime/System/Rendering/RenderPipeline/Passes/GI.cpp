#include "GI.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Runtime/Object/Component/Camera.hpp"
#include "Engine/Runtime/System/Rendering/GPUDriven/GPUDrivenManager.hpp"
#include "Engine/Runtime/System/Rendering/RenderingData.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"
#include <cstring>

namespace Rendering::Passes
{

static float Halton(int index, int base)
{
    float result = 0.0f;
    float f = 1.0f / base;
    int i = index;
    while (i > 0)
    {
        result += f * (i % base);
        i = i / base;
        f = f / base;
    }
    return result;
}

GI::GI()
{
    giShader = ShaderLibrary::GetShader(Shaders::GI_GI);
    mat.SetShader(giShader);

    resolveShader = ShaderLibrary::GetShader(Shaders::GI_Resolve);
    resolveMat.SetShader(resolveShader);

    temporalShader = ShaderLibrary::GetShader(Shaders::GI_Temporal);
    temporalMat.SetShader(temporalShader);

    varianceShader = ShaderLibrary::GetShader(Shaders::GI_VariancePrefilter);
    varianceMat.SetShader(varianceShader);

    atrousShader = ShaderLibrary::GetShader(Shaders::GI_ATrous);

    // Generate Halton sequence (bases 2 & 3) and upload to GPU
    constexpr int haltonLength = 32;
    glm::vec2 haltonData[haltonLength];
    for (int i = 0; i < haltonLength; ++i)
        haltonData[i] = glm::vec2(Halton(i + 1, 2), Halton(i + 1, 3));

    haltonBuffer = GetGfxDriver()->CreateBuffer(
        sizeof(haltonData),
        Gfx::BufferUsage::Storage,
        true,
        false,
        "GI_HaltonSequence"
    );
    memcpy(haltonBuffer->GetCPUVisibleAddress(), haltonData, sizeof(haltonData));
}

void GI::EnsureHistoryBuffers(int width, int height)
{
    int quarterWidth = 4;
    int quarterHeight = 4;
    GetQuarterSize(width, height, quarterWidth, quarterHeight);

    if (historySize.x == quarterWidth && historySize.y == quarterHeight)
        return;

    historySize = {quarterWidth, quarterHeight};
    historyValid = false;
    svgfHistoryValid = false;

    auto usage = Gfx::ImageUsage::Texture | Gfx::ImageUsage::Storage |
                 Gfx::ImageUsage::TransferDst | Gfx::ImageUsage::ColorAttachment;

    // Quarter-resolution SH history
    Gfx::ImageDescription shDesc(quarterWidth, quarterHeight, Gfx::GfxFormat::R16G16B16A16_SFloat);

    historySH0 = GetGfxDriver()->CreateImage(shDesc, usage);
    historySH0->SetName("GI_HistorySH0");
    historySH1 = GetGfxDriver()->CreateImage(shDesc, usage);
    historySH1->SetName("GI_HistorySH1");
    historySH2 = GetGfxDriver()->CreateImage(shDesc, usage);
    historySH2->SetName("GI_HistorySH2");

    GetGfxDriver()->InitGfxImage(*historySH0, float4(0, 0, 0, 0));
    GetGfxDriver()->InitGfxImage(*historySH1, float4(0, 0, 0, 0));
    GetGfxDriver()->InitGfxImage(*historySH2, float4(0, 0, 0, 0));

    Gfx::ImageDescription accumDesc(quarterWidth, quarterHeight, Gfx::GfxFormat::R8_UNorm);
    historyAccumulationCount = GetGfxDriver()->CreateImage(accumDesc, usage);
    historyAccumulationCount->SetName("GI_HistoryAccumulationCount");
    GetGfxDriver()->InitGfxImage(*historyAccumulationCount, float4(0, 0, 0, 0));

    // Full-resolution SVGF history
    Gfx::ImageDescription colorDesc(width, height, Gfx::GfxFormat::R16G16B16A16_SFloat);
    Gfx::ImageDescription depthDesc(width, height, Gfx::GfxFormat::R32_SFloat);

    historyColor = GetGfxDriver()->CreateImage(colorDesc, usage);
    historyColor->SetName("GI_HistoryColor");
    historyMoments = GetGfxDriver()->CreateImage(colorDesc, usage);
    historyMoments->SetName("GI_HistoryMoments");
    historyDepth = GetGfxDriver()->CreateImage(depthDesc, usage);
    historyDepth->SetName("GI_HistoryDepth");
    historyNormal = GetGfxDriver()->CreateImage(colorDesc, usage);
    historyNormal->SetName("GI_HistoryNormal");

    GetGfxDriver()->InitGfxImage(*historyColor, float4(0, 0, 0, 0));
    GetGfxDriver()->InitGfxImage(*historyMoments, float4(0, 0, 0, 0));
    GetGfxDriver()->InitGfxImage(*historyDepth, float4(0, 0, 0, 0));
    GetGfxDriver()->InitGfxImage(*historyNormal, float4(0, 0, 0, 0));
}

void GI::Execute(
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
    if (!setting->gi.enabled || tlas == -1 || rtContext == nullptr)
        return;

    cmd->BeginLabel("GI", {0.2, 0.7, 0.4, 1.0});

    int width = renderingData.screenSize.x;
    int height = renderingData.screenSize.y;
    int quarterWidth = 4;
    int quarterHeight = 4;
    GetQuarterSize(width, height, quarterWidth, quarterHeight);

    EnsureHistoryBuffers(width, height);

    glm::float4 rtSize(width, height, 1.0f / width, 1.0f / height);

    // =========================================================================
    // Pass 1: Quarter-res SH gathering (inline ray tracing)
    // =========================================================================
    cmd->BeginLabel("GI_SH", {0.2, 0.7, 0.4, 1.0});

    Gfx::RenderImageDescriptor shDesc(quarterWidth, quarterHeight, Gfx::GfxFormat::R16G16B16A16_SFloat);
    shDesc.SetRandomWrite(true);

    cmd->AllocateAttachment(giSH0, shDesc);
    cmd->AllocateAttachment(giSH1, shDesc);
    cmd->AllocateAttachment(giSH2, shDesc);

    Gfx::RenderImageDescriptor accumCountDesc(quarterWidth, quarterHeight, Gfx::GfxFormat::R8_UNorm);
    accumCountDesc.SetRandomWrite(true);
    cmd->AllocateAttachment(giAccumulationCount, accumCountDesc);

    // Allocate full-res debug texture for s2h output (xyz = color, w = depth)
    // Gfx::RenderImageDescriptor s2hDebugDesc(width, height, Gfx::GfxFormat::R32G32B32A32_SFloat);
    // s2hDebugDesc.SetRandomWrite(true);
    // cmd->AllocateAttachment(giS2HDebug, s2hDebugDesc);
    // cmd->ClearColorImage(GetGfxDriver()->GetImageFromRenderGraph(giS2HDebug), Gfx::ClearColor{.float32 = {0, 0, 0, 0}});

    mat.SetTexture("hierarchyDepth", GetGfxDriver()->GetImageFromRenderGraph(hizTex));
    mat.SetTexture("albedoTex", GetGfxDriver()->GetImageFromRenderGraph(albedoTex));
    mat.SetTexture("normalTex", GetGfxDriver()->GetImageFromRenderGraph(normalTex));
    mat.SetTexture("noiseTex", renderingData.blueNoise.GetNoiseTexture());
    mat.SetBuffer("haltonSeq", haltonBuffer.get());
    mat.SetTexture("motionVectorTex", GetGfxDriver()->GetImageFromRenderGraph(motionVectorTex));

    mat.SetTexture("historySH0Tex", historySH0.get());
    mat.SetTexture("historySH1Tex", historySH1.get());
    mat.SetTexture("historySH2Tex", historySH2.get());
    mat.SetTexture("historyAccumTex", historyAccumulationCount.get());

    mat.SetTexture("historyDepthTex", historyDepth.get());
    mat.SetTexture("historyNormalTex", historyNormal.get());

    mat.SetTexture("outSH0Tex", GetGfxDriver()->GetImageFromRenderGraph(giSH0));
    mat.SetTexture("outSH1Tex", GetGfxDriver()->GetImageFromRenderGraph(giSH1));
    mat.SetTexture("outSH2Tex", GetGfxDriver()->GetImageFromRenderGraph(giSH2));
    mat.SetTexture("outAccumTex", GetGfxDriver()->GetImageFromRenderGraph(giAccumulationCount));

    mat.SetVector("rtSize", rtSize);
    mat.SetFloat("secondary_bounce", setting->gi.secondary_bounce ? 1.0f : 0.0f);

    // mat.SetTexture("outDebugTex", GetGfxDriver()->GetImageFromRenderGraph(giS2HDebug));
    // mat.SetVector("debugPixel", setting->gi.debugPixel);

    auto* giProgram = mat.GetShaderProgram();
    mat.GetShaderResource()->SetAccelerationStructure("sceneBVH", 0, rtContext, tlas);

    cmd->BindResource(0, renderingData.globalResource);
    cmd->BindResource(mat.GetSet(Gfx::DescriptorSetSemantics::Material), mat.GetShaderResource());
    cmd->BindShaderProgram(giProgram, giProgram->GetDefaultShaderConfig());
    cmd->Dispatch((quarterWidth + 7) / 8, (quarterHeight + 7) / 8, 1);

    // Copy SH outputs to history for next frame
    cmd->Blit(Gfx::ImageIdentifier(*GetGfxDriver()->GetImageFromRenderGraph(giSH0)), Gfx::ImageIdentifier(*historySH0));
    cmd->Blit(Gfx::ImageIdentifier(*GetGfxDriver()->GetImageFromRenderGraph(giSH1)), Gfx::ImageIdentifier(*historySH1));
    cmd->Blit(Gfx::ImageIdentifier(*GetGfxDriver()->GetImageFromRenderGraph(giSH2)), Gfx::ImageIdentifier(*historySH2));
    cmd->Blit(Gfx::ImageIdentifier(*GetGfxDriver()->GetImageFromRenderGraph(giAccumulationCount)), Gfx::ImageIdentifier(*historyAccumulationCount));

    historyValid = true;

    cmd->EndLabel(); // GI_SH

    // =========================================================================
    // Pass 2: Full-res SH Resolve
    // =========================================================================
    cmd->BeginLabel("GI_Resolve", {0.3, 0.8, 0.5, 1.0});

    Gfx::RenderImageDescriptor irradianceDesc(width, height, Gfx::GfxFormat::R16G16B16A16_SFloat);
    irradianceDesc.SetRandomWrite(true);

    cmd->AllocateAttachment(giIrradiance, irradianceDesc);

    resolveMat.SetTexture("rtgiSH0Tex", GetGfxDriver()->GetImageFromRenderGraph(giSH0));
    resolveMat.SetTexture("rtgiSH1Tex", GetGfxDriver()->GetImageFromRenderGraph(giSH1));
    resolveMat.SetTexture("rtgiSH2Tex", GetGfxDriver()->GetImageFromRenderGraph(giSH2));
    resolveMat.SetTexture("normalTex", GetGfxDriver()->GetImageFromRenderGraph(normalTex));
    resolveMat.SetTexture("hierarchyDepth", GetGfxDriver()->GetImageFromRenderGraph(hizTex));
    resolveMat.SetTexture("outIrradianceTex", GetGfxDriver()->GetImageFromRenderGraph(giIrradiance));
    resolveMat.SetVector("texelSize", glm::float4(1.0f / width, 1.0f / height, (float)width, (float)height));

    auto* resolveProgram = resolveMat.GetShaderProgram();
    cmd->BindResource(0, renderingData.globalResource);
    cmd->BindResource(resolveMat.GetSet(Gfx::DescriptorSetSemantics::Material), resolveMat.GetShaderResource());
    cmd->BindShaderProgram(resolveProgram, resolveProgram->GetDefaultShaderConfig());
    cmd->Dispatch((width + 7) / 8, (height + 7) / 8, 1);

    cmd->EndLabel(); // GI_Resolve

    // =========================================================================
    // Pass 3–5: SVGF Denoising (optional)
    // =========================================================================
    if (setting->gi.svgf.enabled)
    {
        glm::float4 svgfParams(width, height, 1.0f / width, 1.0f / height);

        // --- Temporal accumulation ---
        cmd->BeginLabel("GI_Temporal", {0.4, 0.6, 0.8, 1.0});

        cmd->AllocateAttachment(giTemporalOut, irradianceDesc);
        cmd->AllocateAttachment(giMomentsOut, irradianceDesc);

        temporalMat.SetTexture("rtgiRawTex", GetGfxDriver()->GetImageFromRenderGraph(giIrradiance));
        temporalMat.SetTexture("historyColorTex", historyColor.get());
        temporalMat.SetTexture("historyMomentsTex", historyMoments.get());
        temporalMat.SetTexture("motionVecTex", GetGfxDriver()->GetImageFromRenderGraph(motionVectorTex));
        temporalMat.SetTexture("depthTex", GetGfxDriver()->GetImageFromRenderGraph(hizTex));
        temporalMat.SetTexture("historyDepthTex", historyDepth.get());
        temporalMat.SetTexture("normalTex", GetGfxDriver()->GetImageFromRenderGraph(normalTex));
        temporalMat.SetTexture("historyNormalTex", historyNormal.get());
        temporalMat.SetTexture("outAccumulated", GetGfxDriver()->GetImageFromRenderGraph(giTemporalOut));
        temporalMat.SetTexture("outMoments", GetGfxDriver()->GetImageFromRenderGraph(giMomentsOut));
        temporalMat.SetVector("rtgiParams", svgfParams);
        temporalMat.SetVector("temporalParams", glm::float4(setting->gi.svgf.temporalAlpha, svgfHistoryValid ? 1.0f : 0.0f, 0, 0));

        auto* temporalProgram = temporalMat.GetShaderProgram();
        cmd->BindResource(0, renderingData.globalResource);
        cmd->BindResource(temporalMat.GetSet(Gfx::DescriptorSetSemantics::Material), temporalMat.GetShaderResource());
        cmd->BindShaderProgram(temporalProgram, temporalProgram->GetDefaultShaderConfig());
        cmd->Dispatch((width + 7) / 8, (height + 7) / 8, 1);

        cmd->EndLabel(); // GI_Temporal

        // --- Variance prefilter ---
        cmd->BeginLabel("GI_VariancePrefilter", {0.4, 0.6, 0.8, 1.0});

        cmd->AllocateAttachment(giVarianceOut, irradianceDesc);

        varianceMat.SetTexture("inTex", GetGfxDriver()->GetImageFromRenderGraph(giTemporalOut));
        varianceMat.SetTexture("outTex", GetGfxDriver()->GetImageFromRenderGraph(giVarianceOut));
        varianceMat.SetVector("rtgiParams", svgfParams);

        auto* varianceProgram = varianceMat.GetShaderProgram();
        // VariancePrefilter has no [Global] block — bind material only
        cmd->BindResource(varianceMat.GetSet(Gfx::DescriptorSetSemantics::Material), varianceMat.GetShaderResource());
        cmd->BindShaderProgram(varianceProgram, varianceProgram->GetDefaultShaderConfig());
        cmd->Dispatch((width + 7) / 8, (height + 7) / 8, 1);

        cmd->EndLabel(); // GI_VariancePrefilter

        // --- À-trous filter iterations (ping-pong) ---
        Gfx::ImageIdentifier atrousInput = giVarianceOut;
        Gfx::ImageIdentifier svgfResult = giVarianceOut;

        struct GIATrousPushConstant
        {
            glm::vec4 rtgiParams;   // (width, height, invWidth, invHeight)
            glm::vec4 filterParams; // (stepSize, sigmaDepth, sigmaNormal, sigmaLuminance)
        };

        for (int i = 0; i < setting->gi.svgf.atrousIterations; ++i)
        {
            cmd->BeginLabel(("GI_ATrous_" + std::to_string(i)).c_str(), {0.5, 0.7, 0.9, 1.0});

            Gfx::ImageIdentifier atrousOutput = (i % 2 == 0) ? giAtrousA : giAtrousB;
            cmd->AllocateAttachment(atrousOutput, irradianceDesc);

            int stepSize = 1 << i;
            GIATrousPushConstant pconst;
            pconst.rtgiParams = svgfParams;
            pconst.filterParams = glm::float4((float)stepSize, setting->gi.svgf.sigmaDepth, setting->gi.svgf.sigmaNormal, setting->gi.svgf.sigmaLuminance);

            auto* atrousProgram = atrousShader->GetShaderProgram();

            std::vector<Gfx::DynamicBinding> bindings = {
                Gfx::DynamicBinding("inTex", atrousInput),
                Gfx::DynamicBinding("depthTex", hizTex),
                Gfx::DynamicBinding("normalTex", normalTex),
                Gfx::DynamicBinding("outTex", atrousOutput)
            };

            cmd->BindShaderProgram(atrousProgram, atrousProgram->GetDefaultShaderConfig());
            cmd->SetPushConstant(atrousProgram, &pconst);
            cmd->BindResource(0, renderingData.globalResource);
            cmd->BindResource(1, bindings);
            cmd->Dispatch((width + 7) / 8, (height + 7) / 8, 1);

            atrousInput = atrousOutput;
            svgfResult = atrousOutput;

            cmd->EndLabel(); // GI_ATrous_i
        }

        giOutput = svgfResult;

        // Save SVGF history for next frame
        cmd->Blit(Gfx::ImageIdentifier(*GetGfxDriver()->GetImageFromRenderGraph(giTemporalOut)), Gfx::ImageIdentifier(*historyColor));
        cmd->Blit(Gfx::ImageIdentifier(*GetGfxDriver()->GetImageFromRenderGraph(giMomentsOut)), Gfx::ImageIdentifier(*historyMoments));
        svgfHistoryValid = true;
    }
    else
    {
        giOutput = giIrradiance;
        svgfHistoryValid = false;
    }

    // Unconditionally preserve depth/normal history for next frame's GI_SH disocclusion check
    cmd->Blit(Gfx::ImageIdentifier(*GetGfxDriver()->GetImageFromRenderGraph(hizTex)), Gfx::ImageIdentifier(*historyDepth));
    cmd->Blit(Gfx::ImageIdentifier(*GetGfxDriver()->GetImageFromRenderGraph(normalTex)), Gfx::ImageIdentifier(*historyNormal));

    debugGI = setting->gi.debug_giOutput;

    cmd->EndLabel(); // GI
}

bool GI::DebugBlit(Gfx::ImageIdentifier& dst)
{
    if (debugGI)
    {
        dst = giOutput;
        return true;
    }
    return false;
}

} // namespace Rendering::Passes
