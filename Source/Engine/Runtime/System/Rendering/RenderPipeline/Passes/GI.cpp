#include "GI.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Runtime/Object/Component/Camera.hpp"
#include "Engine/Runtime/System/Rendering/GPUDriven/GPUDrivenManager.hpp"
#include "Engine/Runtime/System/Rendering/RenderingData.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace Rendering::Passes
{

namespace
{
struct PostBlurInput
{
    glm::float4 rtSize = glm::float4(0.0f);
    float distanceScale = 0.0f;
    float probeDownsample = 2.0f;
    glm::float2 padding0 = glm::float2(0.0f);
    glm::float4 blurParams = glm::float4(0.0f);
    glm::float4 atrousParams = glm::float4(0.0f);
    float minNormalDot = 0.0f;
    glm::float3 padding1 = glm::float3(0.0f);
};
} // namespace

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

uint32_t GI::ComputeAdaptiveRayCount(float accumRatio, uint32_t lowRayCount, uint32_t maxRayCount, float stableAccumFrames)
{
    lowRayCount = std::max(lowRayCount, 1u);
    maxRayCount = std::max(maxRayCount, 1u);
    if (lowRayCount > maxRayCount)
        std::swap(lowRayCount, maxRayCount);

    const float clampedStableFrames = std::max(stableAccumFrames, 1.0f);
    const float accumFrames = std::clamp(accumRatio, 0.0f, 1.0f) * 48.0f;
    const float t = std::clamp(accumFrames / clampedStableFrames, 0.0f, 1.0f);
    const float rayCount = std::lerp((float)maxRayCount, (float)lowRayCount, t);
    return std::clamp((uint32_t)std::lround(rayCount), lowRayCount, maxRayCount);
}

GI::GI()
{
    giRayGenShader = ShaderLibrary::GetShader(Shaders::GI_RayGen);
    rayGenMat.SetShader(giRayGenShader);

    probePackShader = ShaderLibrary::GetShader(Shaders::GI_ProbePack);
    probePackMat.SetShader(probePackShader);

    giDisocclusionShader = ShaderLibrary::GetShader(Shaders::GI_Disocclusion);
    disocclusionMat.SetShader(giDisocclusionShader);

    giShader = ShaderLibrary::GetShader(Shaders::GI_GI);
    mat.SetShader(giShader);

    blurShader = ShaderLibrary::GetShader(Shaders::GI_Blur);
    blurMat.SetShader(blurShader);

    postBlurShader = ShaderLibrary::GetShader(Shaders::GI_PostBlur);

    resolveShader = ShaderLibrary::GetShader(Shaders::GI_Resolve);
    resolveMat.SetShader(resolveShader);

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

void GI::EnsureHistoryBuffers(int width, int height, uint32_t probeDownsample)
{
    int probeWidth = 0;
    int probeHeight = 0;
    GetProbeAtlasSize(width, height, probeDownsample, probeWidth, probeHeight);

    if (historySize.x == probeWidth && historySize.y == probeHeight && historyFullResSize.x == width && historyFullResSize.y == height)
        return;

    historySize = {probeWidth, probeHeight};
    historyFullResSize = {width, height};

    auto usage = Gfx::ImageUsage::Texture | Gfx::ImageUsage::Storage |
                 Gfx::ImageUsage::TransferDst | Gfx::ImageUsage::ColorAttachment;

    // Probe-resolution SH history.
    Gfx::ImageDescription shDesc(probeWidth, probeHeight, Gfx::GfxFormat::R16G16B16A16_SFloat);

    historySH0 = GetGfxDriver()->CreateImage(shDesc, usage);
    historySH0->SetName("GI_HistorySH0");
    historySH1 = GetGfxDriver()->CreateImage(shDesc, usage);
    historySH1->SetName("GI_HistorySH1");
    historySH2 = GetGfxDriver()->CreateImage(shDesc, usage);
    historySH2->SetName("GI_HistorySH2");

    GetGfxDriver()->InitGfxImage(*historySH0, float4(0, 0, 0, 0));
    GetGfxDriver()->InitGfxImage(*historySH1, float4(0, 0, 0, 0));
    GetGfxDriver()->InitGfxImage(*historySH2, float4(0, 0, 0, 0));

    Gfx::ImageDescription accumDesc(probeWidth, probeHeight, Gfx::GfxFormat::R8_UNorm);
    historyAccumulationCount = GetGfxDriver()->CreateImage(accumDesc, usage);
    historyAccumulationCount->SetName("GI_HistoryAccumulationCount");
    GetGfxDriver()->InitGfxImage(*historyAccumulationCount, float4(0, 0, 0, 0));

    Gfx::ImageDescription depthDesc(width, height, Gfx::GfxFormat::R32_SFloat);

    historyDepth = GetGfxDriver()->CreateImage(depthDesc, usage);
    historyDepth->SetName("GI_HistoryDepth");
    historyNormal = GetGfxDriver()->CreateImage(
        Gfx::ImageDescription(width, height, Gfx::GfxFormat::A2B10G10R10_UNorm),
        usage
    );
    historyNormal->SetName("GI_HistoryNormal");
    historyLuminance = GetGfxDriver()->CreateImage(
        Gfx::ImageDescription(width, height, Gfx::GfxFormat::R32_SFloat),
        usage
    );
    historyLuminance->SetName("GI_HistoryLuminance");

    GetGfxDriver()->InitGfxImage(*historyDepth, float4(0, 0, 0, 0));
    GetGfxDriver()->InitGfxImage(*historyNormal, float4(0, 0, 0, 0));
    GetGfxDriver()->InitGfxImage(*historyLuminance, float4(0, 0, 0, 0));
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
    (void)albedoTex;

    debugGI = setting->gi.enabled && setting->gi.debug_giOutput;
    debugGIAccumulation = setting->gi.enabled && setting->gi.debug_giAccumulationOutput;

    if (!setting->gi.enabled || tlas == -1 || rtContext == nullptr)
        return;

    cmd->BeginLabel("GI", {0.2, 0.7, 0.4, 1.0});

    int width = renderingData.screenSize.x;
    int height = renderingData.screenSize.y;
    uint32_t probeDownsample = setting->gi.probeDownsample == 4 ? 4u : 2u;
    int probeWidth = 0;
    int probeHeight = 0;
    int rayWidth = 0;
    int rayHeight = 0;
    GetProbeAtlasSize(width, height, probeDownsample, probeWidth, probeHeight);
    GetRayAtlasSize(width, height, rayWidth, rayHeight);

    EnsureHistoryBuffers(width, height, probeDownsample);

    glm::float4 rtSize(width, height, 1.0f / width, 1.0f / height);
    float distanceScale = 0.0f;
    if (renderingData.mainCamera != nullptr)
    {
        const float projectedCellPixels = std::max(setting->gi.adaptivePlaneProjectedCellPixels, 0.0f);
        if (projectedCellPixels > 0.0f && height > 0)
        {
            float dd = glm::max(1.0f / height, (float)height / (width * width));
            if (renderingData.mainCamera->GetProjectionMode() == Camera::ProjectionMode::Perspective)
            {
                const float fovY = renderingData.mainCamera->GetFoV();
                // distanceScale = 2.0f * std::tan(fovY * 0.5f) * projectedCellPixels * dd;
                distanceScale = std::tan(fovY * projectedCellPixels * dd);
            }
            else
            {
                distanceScale = 2.0f * renderingData.mainCamera->GetProjectionTop() * projectedCellPixels * dd; // TODO: this needs better handling
            }
        }
    }

    // =========================================================================
    // Pass 1: Sparse ray generation (one ray per 4x4 footprint)
    // =========================================================================
    cmd->BeginLabel("GI_RayGen", {0.2, 0.7, 0.4, 1.0});

    Gfx::RenderImageDescriptor rayDataDesc(rayWidth, rayHeight, Gfx::GfxFormat::R16G16B16A16_SFloat);
    rayDataDesc.SetRandomWrite(true);
    cmd->AllocateAttachment(giRayData, rayDataDesc);

    Gfx::RenderImageDescriptor rayMetaDesc(rayWidth, rayHeight, Gfx::GfxFormat::R16G16B16A16_SFloat);
    rayMetaDesc.SetRandomWrite(true);
    cmd->AllocateAttachment(giRayMeta, rayMetaDesc);

    rayGenMat.SetTexture("hierarchyDepth", GetGfxDriver()->GetImageFromRenderGraph(hizTex));
    rayGenMat.SetTexture("normalTex", GetGfxDriver()->GetImageFromRenderGraph(normalTex));
    rayGenMat.SetTexture("motionVectorTex", GetGfxDriver()->GetImageFromRenderGraph(motionVectorTex));
    rayGenMat.SetTexture("historyAccumTex", historyAccumulationCount.get());
    rayGenMat.SetTexture("historyDepthTex", historyDepth.get());
    rayGenMat.SetTexture("historyNormalTex", historyNormal.get());
    rayGenMat.SetTexture("noiseTex", renderingData.blueNoise.GetNoiseTexture());
    rayGenMat.SetBuffer("haltonSeq", haltonBuffer.get());
    rayGenMat.SetTexture("outRayDataTex", GetGfxDriver()->GetImageFromRenderGraph(giRayData));
    rayGenMat.SetTexture("outRayMetaTex", GetGfxDriver()->GetImageFromRenderGraph(giRayMeta));
    rayGenMat.SetVector("rtSize", rtSize);
    rayGenMat.SetFloat("probeDownsample", (float)probeDownsample);
    rayGenMat.SetFloat("secondary_bounce", setting->gi.secondary_bounce ? 1.0f : 0.0f);
    rayGenMat.SetFloat("lowRayCount", (float)setting->gi.lowRayCount);
    rayGenMat.SetFloat("maxRayCount", (float)setting->gi.maxRayCount);
    rayGenMat.SetFloat("stableAccumFrames", setting->gi.stableAccumFrames);
    rayGenMat.SetFloat("distanceScale", distanceScale);

    auto* rayGenProgram = rayGenMat.GetShaderProgram();
    rayGenMat.GetShaderResource()->SetAccelerationStructure("sceneBVH", 0, rtContext, tlas);

    cmd->BindResource(0, renderingData.globalResource);
    cmd->BindResource(rayGenMat.GetSet(Gfx::DescriptorSetSemantics::Material), rayGenMat.GetShaderResource());
    cmd->BindShaderProgram(rayGenProgram, rayGenProgram->GetDefaultShaderConfig());
    cmd->Dispatch((rayWidth + 7) / 8, (rayHeight + 7) / 8, 1);

    cmd->EndLabel(); // GI_RayGen

    // =========================================================================
    // Pass 2: Probe-res geometry packing
    // =========================================================================
    cmd->BeginLabel("GI_ProbePack", {0.205, 0.705, 0.405, 1.0});

    Gfx::RenderImageDescriptor probeGeometryDesc(probeWidth, probeHeight, Gfx::GfxFormat::R32G32_UInt);
    probeGeometryDesc.SetRandomWrite(true);
    cmd->AllocateAttachment(giProbeGeometry, probeGeometryDesc);
    cmd->AllocateAttachment(giHistoryProbeGeometry, probeGeometryDesc);

    Gfx::RenderImageDescriptor probeMotionDesc(probeWidth, probeHeight, Gfx::GfxFormat::R16G16_SFloat);
    probeMotionDesc.SetRandomWrite(true);
    cmd->AllocateAttachment(giProbeMotion, probeMotionDesc);

    probePackMat.SetTexture("hierarchyDepth", GetGfxDriver()->GetImageFromRenderGraph(hizTex));
    probePackMat.SetTexture("normalTex", GetGfxDriver()->GetImageFromRenderGraph(normalTex));
    probePackMat.SetTexture("motionVectorTex", GetGfxDriver()->GetImageFromRenderGraph(motionVectorTex));
    probePackMat.SetTexture("historyDepthTex", historyDepth.get());
    probePackMat.SetTexture("historyNormalTex", historyNormal.get());
    probePackMat.SetTexture("outProbeGeometryTex", GetGfxDriver()->GetImageFromRenderGraph(giProbeGeometry));
    probePackMat.SetTexture("outProbeMotionTex", GetGfxDriver()->GetImageFromRenderGraph(giProbeMotion));
    probePackMat.SetTexture("outHistoryProbeGeometryTex", GetGfxDriver()->GetImageFromRenderGraph(giHistoryProbeGeometry));
    probePackMat.SetVector("rtSize", rtSize);
    probePackMat.SetFloat("probeDownsample", (float)probeDownsample);

    auto* probePackProgram = probePackMat.GetShaderProgram();
    cmd->BindResource(probePackMat.GetSet(Gfx::DescriptorSetSemantics::Material), probePackMat.GetShaderResource());
    cmd->BindShaderProgram(probePackProgram, probePackProgram->GetDefaultShaderConfig());
    cmd->Dispatch((probeWidth + 7) / 8, (probeHeight + 7) / 8, 1);

    cmd->EndLabel(); // GI_ProbePack

    // =========================================================================
    // Pass 3: Probe-res disocclusion classification
    // =========================================================================
    cmd->BeginLabel("GI_Disocclusion", {0.21, 0.71, 0.41, 1.0});

    Gfx::RenderImageDescriptor disocclusionDesc(probeWidth, probeHeight, Gfx::GfxFormat::R8_UNorm);
    disocclusionDesc.SetRandomWrite(true);
    cmd->AllocateAttachment(giDisocclusionMask, disocclusionDesc);

    disocclusionMat.SetTexture("probeGeometryTex", GetGfxDriver()->GetImageFromRenderGraph(giProbeGeometry));
    disocclusionMat.SetTexture("motionVectorTex", GetGfxDriver()->GetImageFromRenderGraph(giProbeMotion));
    disocclusionMat.SetTexture("historyProbeGeometryTex", GetGfxDriver()->GetImageFromRenderGraph(giHistoryProbeGeometry));
    disocclusionMat.SetTexture("outDisocclusionMaskTex", GetGfxDriver()->GetImageFromRenderGraph(giDisocclusionMask));
    disocclusionMat.SetVector("rtSize", rtSize);
    disocclusionMat.SetFloat("probeDownsample", (float)probeDownsample);
    disocclusionMat.SetFloat("distanceScale", distanceScale);

    auto* disocclusionProgram = disocclusionMat.GetShaderProgram();
    cmd->BindResource(0, renderingData.globalResource);
    cmd->BindResource(disocclusionMat.GetSet(Gfx::DescriptorSetSemantics::Material), disocclusionMat.GetShaderResource());
    cmd->BindShaderProgram(disocclusionProgram, disocclusionProgram->GetDefaultShaderConfig());
    cmd->Dispatch((probeWidth + 7) / 8, (probeHeight + 7) / 8, 1);

    cmd->EndLabel(); // GI_Disocclusion

    // =========================================================================
    // Pass 4: Probe-res SH gathering + history blend
    // =========================================================================
    cmd->BeginLabel("GI_SH", {0.2, 0.7, 0.4, 1.0});

    Gfx::RenderImageDescriptor shDesc(probeWidth, probeHeight, Gfx::GfxFormat::R16G16B16A16_SFloat);
    shDesc.SetRandomWrite(true);

    cmd->AllocateAttachment(giSH0, shDesc);
    cmd->AllocateAttachment(giSH1, shDesc);
    cmd->AllocateAttachment(giSH2, shDesc);

    Gfx::RenderImageDescriptor accumCountDesc(probeWidth, probeHeight, Gfx::GfxFormat::R8_UNorm);
    accumCountDesc.SetRandomWrite(true);
    cmd->AllocateAttachment(giAccumulationCount, accumCountDesc);

    mat.SetTexture("probeGeometryTex", GetGfxDriver()->GetImageFromRenderGraph(giProbeGeometry));
    mat.SetTexture("motionVectorTex", GetGfxDriver()->GetImageFromRenderGraph(giProbeMotion));
    mat.SetTexture("disocclusionMaskTex", GetGfxDriver()->GetImageFromRenderGraph(giDisocclusionMask));
    mat.SetTexture("rayDataTex", GetGfxDriver()->GetImageFromRenderGraph(giRayData));
    mat.SetTexture("rayMetaTex", GetGfxDriver()->GetImageFromRenderGraph(giRayMeta));
    mat.SetTexture("noiseTex", renderingData.blueNoise.GetNoiseTexture());

    mat.SetTexture("historySH0Tex", historySH0.get());
    mat.SetTexture("historySH1Tex", historySH1.get());
    mat.SetTexture("historySH2Tex", historySH2.get());
    mat.SetTexture("historyAccumTex", historyAccumulationCount.get());
    mat.SetTexture("historyProbeGeometryTex", GetGfxDriver()->GetImageFromRenderGraph(giHistoryProbeGeometry));

    mat.SetTexture("outSH0Tex", GetGfxDriver()->GetImageFromRenderGraph(giSH0));
    mat.SetTexture("outSH1Tex", GetGfxDriver()->GetImageFromRenderGraph(giSH1));
    mat.SetTexture("outSH2Tex", GetGfxDriver()->GetImageFromRenderGraph(giSH2));
    mat.SetTexture("outAccumTex", GetGfxDriver()->GetImageFromRenderGraph(giAccumulationCount));
    mat.SetVector("rtSize", rtSize);
    mat.SetFloat("probeDownsample", (float)probeDownsample);
    mat.SetFloat("distanceScale", distanceScale);

    auto* giProgram = mat.GetShaderProgram();
    cmd->BindResource(0, renderingData.globalResource);
    cmd->BindResource(mat.GetSet(Gfx::DescriptorSetSemantics::Material), mat.GetShaderResource());
    cmd->BindShaderProgram(giProgram, giProgram->GetDefaultShaderConfig());
    cmd->Dispatch((probeWidth + 7) / 8, (probeHeight + 7) / 8, 1);

    cmd->EndLabel(); // GI_SH

    // =========================================================================
    // Pass 5: Probe-atlas recurrent blur
    // =========================================================================
    cmd->BeginLabel("GI_Blur", {0.25, 0.75, 0.45, 1.0});

    Gfx::RenderImageDescriptor blurredShDesc(probeWidth, probeHeight, Gfx::GfxFormat::R16G16B16A16_SFloat);
    blurredShDesc.SetRandomWrite(true);

    cmd->AllocateAttachment(giBlurredSH0, blurredShDesc);
    cmd->AllocateAttachment(giBlurredSH1, blurredShDesc);
    cmd->AllocateAttachment(giBlurredSH2, blurredShDesc);

    blurMat.SetTexture("probeGeometryTex", GetGfxDriver()->GetImageFromRenderGraph(giProbeGeometry));
    blurMat.SetTexture("disocclusionMaskTex", GetGfxDriver()->GetImageFromRenderGraph(giDisocclusionMask));
    blurMat.SetTexture("inSH0Tex", GetGfxDriver()->GetImageFromRenderGraph(giSH0));
    blurMat.SetTexture("inSH1Tex", GetGfxDriver()->GetImageFromRenderGraph(giSH1));
    blurMat.SetTexture("inSH2Tex", GetGfxDriver()->GetImageFromRenderGraph(giSH2));
    blurMat.SetTexture("accumTex", GetGfxDriver()->GetImageFromRenderGraph(giAccumulationCount));
    blurMat.SetTexture("outSH0Tex", GetGfxDriver()->GetImageFromRenderGraph(giBlurredSH0));
    blurMat.SetTexture("outSH1Tex", GetGfxDriver()->GetImageFromRenderGraph(giBlurredSH1));
    blurMat.SetTexture("outSH2Tex", GetGfxDriver()->GetImageFromRenderGraph(giBlurredSH2));
    blurMat.SetVector("rtSize", rtSize);
    blurMat.SetFloat("probeDownsample", (float)probeDownsample);
    blurMat.SetFloat("distanceScale", distanceScale);
    blurMat.SetVector(
        "blurParams",
        glm::float4(
            (float)setting->gi.probeBlur.minRadius,
            (float)setting->gi.probeBlur.maxRadius,
            (float)setting->gi.probeBlur.stableAccumFrames,
            setting->gi.probeBlur.planeSigma
        )
    );
    blurMat.SetFloat("minNormalDot", setting->gi.probeBlur.minNormalDot);

    auto* blurProgram = blurMat.GetShaderProgram();
    cmd->BindResource(0, renderingData.globalResource);
    cmd->BindResource(blurMat.GetSet(Gfx::DescriptorSetSemantics::Material), blurMat.GetShaderResource());
    cmd->BindShaderProgram(blurProgram, blurProgram->GetDefaultShaderConfig());
    cmd->Dispatch((probeWidth + 7) / 8, (probeHeight + 7) / 8, 1);

    cmd->Blit(Gfx::ImageIdentifier(*GetGfxDriver()->GetImageFromRenderGraph(giBlurredSH0)), Gfx::ImageIdentifier(*historySH0));
    cmd->Blit(Gfx::ImageIdentifier(*GetGfxDriver()->GetImageFromRenderGraph(giBlurredSH1)), Gfx::ImageIdentifier(*historySH1));
    cmd->Blit(Gfx::ImageIdentifier(*GetGfxDriver()->GetImageFromRenderGraph(giBlurredSH2)), Gfx::ImageIdentifier(*historySH2));

    cmd->EndLabel(); // GI_Blur

    // =========================================================================
    // Pass 6: Probe-atlas a-trous post filter
    // =========================================================================
    cmd->BeginLabel("GI_PostBlur", {0.28, 0.78, 0.48, 1.0});

    auto radiusToAtrousPassCount = [](int radius) {
        int clampedRadius = std::max(radius, 0);
        if (clampedRadius <= 0)
            return 0;

        int passCount = 0;
        int coveredRadius = 0;
        int stepSize = 1;
        while (coveredRadius < clampedRadius)
        {
            coveredRadius += stepSize * 2;
            stepSize <<= 1;
            ++passCount;
        }
        return passCount;
    };

    const int stablePassCount = radiusToAtrousPassCount(setting->gi.postBlur.minRadius);
    const int unstablePassCount = radiusToAtrousPassCount(std::max(setting->gi.postBlur.maxRadius, setting->gi.postBlur.minRadius));
    const int totalIterationCount = std::max(stablePassCount, unstablePassCount);

    Gfx::ImageIdentifier postBlurSH0 = giBlurredSH0;
    Gfx::ImageIdentifier postBlurSH1 = giBlurredSH1;
    Gfx::ImageIdentifier postBlurSH2 = giBlurredSH2;
    if (setting->gi.enablePostBlur && totalIterationCount > 0)
    {
        if (postBlurPassResources.size() < (size_t)totalIterationCount)
            postBlurPassResources.resize(totalIterationCount);

        auto* postBlurProgram = postBlurShader->GetShaderProgram();
        int postBlurSet = postBlurShader->GetSet(Gfx::DescriptorSetSemantics::Material);
        for (int iteration = 0; iteration < totalIterationCount; ++iteration)
        {
            PostBlurInput inputData;
            inputData.rtSize = rtSize;
            inputData.distanceScale = distanceScale;
            inputData.probeDownsample = (float)probeDownsample;
            inputData.blurParams = glm::float4(
                (float)stablePassCount,
                (float)unstablePassCount,
                (float)setting->gi.postBlur.stableAccumFrames,
                setting->gi.postBlur.planeSigma
            );
            inputData.atrousParams = glm::float4((float)(1 << iteration), (float)iteration, 0.0f, 0.0f);
            inputData.minNormalDot = setting->gi.postBlur.minNormalDot;

            auto& postBlurPassResource = postBlurPassResources[iteration];
            renderingData.pipelineAllocator->AllocateBuffer(postBlurPassResource.inputBuffer, sizeof(PostBlurInput));
            postBlurPassResource.inputBuffer.Write(&inputData, sizeof(PostBlurInput));

            bool writeToPrimarySet = (iteration & 1) == 0;
            Gfx::ImageIdentifier postBlurOutputSH0 = writeToPrimarySet ? giSH0 : giBlurredSH0;
            Gfx::ImageIdentifier postBlurOutputSH1 = writeToPrimarySet ? giSH1 : giBlurredSH1;
            Gfx::ImageIdentifier postBlurOutputSH2 = writeToPrimarySet ? giSH2 : giBlurredSH2;

            cmd->BindResource(0, renderingData.globalResource);
            cmd->BindResource(
                postBlurSet,
                {
                    Gfx::DynamicBinding("perMaterial", *postBlurPassResource.inputBuffer.GetBuffer()),
                    Gfx::DynamicBinding("probeGeometryTex", giProbeGeometry),
                    Gfx::DynamicBinding("inSH0Tex", postBlurSH0),
                    Gfx::DynamicBinding("inSH1Tex", postBlurSH1),
                    Gfx::DynamicBinding("inSH2Tex", postBlurSH2),
                    Gfx::DynamicBinding("accumTex", giAccumulationCount),
                    Gfx::DynamicBinding("outSH0Tex", postBlurOutputSH0),
                    Gfx::DynamicBinding("outSH1Tex", postBlurOutputSH1),
                    Gfx::DynamicBinding("outSH2Tex", postBlurOutputSH2),
                }
            );
            cmd->BindShaderProgram(postBlurProgram, postBlurProgram->GetDefaultShaderConfig());
            cmd->Dispatch((probeWidth + 7) / 8, (probeHeight + 7) / 8, 1);

            postBlurSH0 = postBlurOutputSH0;
            postBlurSH1 = postBlurOutputSH1;
            postBlurSH2 = postBlurOutputSH2;
        }
    }

    cmd->EndLabel(); // GI_PostBlur

    // =========================================================================
    // Pass 7: Full-res SH resolve
    // =========================================================================
    cmd->BeginLabel("GI_Resolve", {0.3, 0.8, 0.5, 1.0});

    Gfx::RenderImageDescriptor irradianceDesc(width, height, Gfx::GfxFormat::R16G16B16A16_SFloat);
    irradianceDesc.SetRandomWrite(true);

    cmd->AllocateAttachment(giIrradiance, irradianceDesc);

    Gfx::RenderImageDescriptor luminanceDesc(width, height, Gfx::GfxFormat::R32_SFloat);
    luminanceDesc.SetRandomWrite(true);
    cmd->AllocateAttachment(giLuminance, luminanceDesc);

    resolveMat.SetTexture("rtgiSH0Tex", GetGfxDriver()->GetImageFromRenderGraph(postBlurSH0));
    resolveMat.SetTexture("rtgiSH1Tex", GetGfxDriver()->GetImageFromRenderGraph(postBlurSH1));
    resolveMat.SetTexture("rtgiSH2Tex", GetGfxDriver()->GetImageFromRenderGraph(postBlurSH2));
    resolveMat.SetTexture("accumTex", GetGfxDriver()->GetImageFromRenderGraph(giAccumulationCount));
    resolveMat.SetTexture("disocclusionMaskTex", GetGfxDriver()->GetImageFromRenderGraph(giDisocclusionMask));
    resolveMat.SetTexture("normalTex", GetGfxDriver()->GetImageFromRenderGraph(normalTex));
    resolveMat.SetTexture("motionVectorTex", GetGfxDriver()->GetImageFromRenderGraph(motionVectorTex));
    resolveMat.SetTexture("hierarchyDepth", GetGfxDriver()->GetImageFromRenderGraph(hizTex));
    resolveMat.SetTexture("historyDepthTex", historyDepth.get());
    resolveMat.SetTexture("historyNormalTex", historyNormal.get());
    resolveMat.SetTexture("historyLuminanceTex", historyLuminance.get());
    resolveMat.SetTexture("historyAccumTex", historyAccumulationCount.get());
    resolveMat.SetTexture("outIrradianceTex", GetGfxDriver()->GetImageFromRenderGraph(giIrradiance));
    resolveMat.SetTexture("outLuminanceTex", GetGfxDriver()->GetImageFromRenderGraph(giLuminance));
    resolveMat.SetVector("texelSize", glm::float4(1.0f / width, 1.0f / height, (float)width, (float)height));
    resolveMat.SetFloat("probeDownsample", (float)probeDownsample);
    resolveMat.SetFloat("distanceScale", distanceScale);
    resolveMat.SetFloat("resolveClampWeightScale", setting->gi.resolveClampWeightScale);

    auto* resolveProgram = resolveMat.GetShaderProgram();
    cmd->BindResource(0, renderingData.globalResource);
    cmd->BindResource(resolveMat.GetSet(Gfx::DescriptorSetSemantics::Material), resolveMat.GetShaderResource());
    cmd->BindShaderProgram(resolveProgram, resolveProgram->GetDefaultShaderConfig());
    cmd->Dispatch((width + 7) / 8, (height + 7) / 8, 1);

    cmd->EndLabel(); // GI_Resolve

    cmd->Blit(Gfx::ImageIdentifier(*GetGfxDriver()->GetImageFromRenderGraph(hizTex)), Gfx::ImageIdentifier(*historyDepth));
    cmd->Blit(Gfx::ImageIdentifier(*GetGfxDriver()->GetImageFromRenderGraph(normalTex)), Gfx::ImageIdentifier(*historyNormal));
    cmd->Blit(Gfx::ImageIdentifier(*GetGfxDriver()->GetImageFromRenderGraph(giLuminance)), Gfx::ImageIdentifier(*historyLuminance));
    cmd->Blit(Gfx::ImageIdentifier(*GetGfxDriver()->GetImageFromRenderGraph(giAccumulationCount)), Gfx::ImageIdentifier(*historyAccumulationCount));

    giOutput = giIrradiance;

    cmd->EndLabel(); // GI
}

bool GI::DebugBlit(Gfx::ImageIdentifier& dst)
{
    if (debugGIAccumulation)
    {
        dst = giAccumulationCount;
        return true;
    }

    if (debugGI)
    {
        dst = giOutput;
        return true;
    }
    return false;
}

} // namespace Rendering::Passes
