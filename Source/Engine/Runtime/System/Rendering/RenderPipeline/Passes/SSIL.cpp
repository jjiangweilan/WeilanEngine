#include "SSIL.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Runtime/Object/Component/Camera.hpp"
#include "Engine/Runtime/System/Rendering/RenderingData.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"

namespace Rendering::Passes
{
SSIL::GeometryPass::GeometryPass()
{
    shader = ShaderLibrary::GetShader(ShaderLibrary::GetShaderName(Shaders::PostProcess_SSILGeometry));
    mat.SetShader(shader);
    mat.SetName("SSIL_Geometry_Material");
}

void SSIL::GeometryPass::Execute(
    Gfx::CommandBuffer* cmd,
    const Gfx::ImageIdentifier& hizTex,
    const Gfx::ImageIdentifier& smoothNormalTex,
    const Gfx::ImageIdentifier& destination,
    glm::int2 halfResSize,
    glm::int2 fullResSize
)
{
    if (halfResSize.x == 0 || halfResSize.y == 0)
        return;

    mat.SetTexture("depthTex", GetGfxDriver()->GetImageFromRenderGraph(hizTex));
    mat.SetTexture("smoothNormalTex", GetGfxDriver()->GetImageFromRenderGraph(smoothNormalTex));
    mat.SetTexture("outGeometryTex", GetGfxDriver()->GetImageFromRenderGraph(destination));

    mat.SetVector(
        "rtSize",
        glm::float4(halfResSize.x, halfResSize.y, 1.0f / halfResSize.x, 1.0f / halfResSize.y)
    );
    mat.SetVector(
        "fullResSize",
        glm::float4(fullResSize.x, fullResSize.y, 1.0f / fullResSize.x, 1.0f / fullResSize.y)
    );

    int dispatchX = (halfResSize.x + 7) / 8;
    int dispatchY = (halfResSize.y + 7) / 8;

    cmd->BindResource(1, mat.GetShaderResource());
    cmd->BindShaderProgram(shader->GetShaderProgram(), shader->GetShaderProgram()->GetDefaultShaderConfig());
    cmd->Dispatch(dispatchX, dispatchY, 1);
}

SSIL::BilateralFilterPass::BilateralFilterPass()
{
    shader = ShaderLibrary::GetShader(ShaderLibrary::GetShaderName(Shaders::PostProcess_SSILBilateralFilter));
    mat.SetShader(shader);
    mat.SetName("SSIL_BilateralFilter_Material");
}

void SSIL::BilateralFilterPass::Execute(
    Gfx::CommandBuffer* cmd,
    const Gfx::ImageIdentifier& sourceTex,
    glm::int2 sourceTexSize,
    glm::int2 highResTexSize,
    const Gfx::ImageIdentifier& lowGeometryTex,
    const Gfx::ImageIdentifier& highDepth,
    const Gfx::ImageIdentifier& highSmoothNormal,
    const Gfx::ImageIdentifier& destination
)
{
    if (sourceTexSize.x == 0 || sourceTexSize.y == 0)
        return;

    mat.SetTexture("lowColor", GetGfxDriver()->GetImageFromRenderGraph(sourceTex));
    mat.SetTexture("lowGeometryTex", GetGfxDriver()->GetImageFromRenderGraph(lowGeometryTex));
    mat.SetTexture("highDepth", GetGfxDriver()->GetImageFromRenderGraph(highDepth));
    mat.SetTexture("highSmoothNormal", GetGfxDriver()->GetImageFromRenderGraph(highSmoothNormal));
    mat.SetTexture("dst", GetGfxDriver()->GetImageFromRenderGraph(destination));

    mat.SetVector(
        "lowResTexSize",
        glm::float4(sourceTexSize.x, sourceTexSize.y, 1.0f / sourceTexSize.x, 1.0f / sourceTexSize.y)
    );
    mat.SetVector(
        "highResTexSize",
        glm::float4(
            highResTexSize.x,
            highResTexSize.y,
            1.0f / highResTexSize.x,
            1.0f / highResTexSize.y
        )
    );

    mat.SetFloat("depthDiffSigma", depthDiffSigma);
    mat.SetFloat("distanceScale", 0.5f);

    int dispatchX = (highResTexSize.x + 7) / 8;
    int dispatchY = (highResTexSize.y + 7) / 8;

    cmd->BindResource(1, mat.GetShaderResource());
    cmd->BindShaderProgram(shader->GetShaderProgram(), shader->GetShaderProgram()->GetDefaultShaderConfig());
    cmd->Dispatch(dispatchX, dispatchY, 1);
}

SSIL::SSIL()
{
    ssilShader = ShaderLibrary::GetShader(Shaders::PostProcess_SSIL);
    mat.SetShader(ssilShader);

    smoothNormalShader = ShaderLibrary::GetShader(Shaders::PostProcess_SSILSmoothNormal);
    smoothNormalMat.SetShader(smoothNormalShader);
    smoothNormalMat.SetName("SSIL_SmoothNormal_Material");

    temporalAccumulationShader = ShaderLibrary::GetShader(Shaders::PostProcess_SSILTemporalAccumulation);
    temporalAccumulationMat.SetShader(temporalAccumulationShader);
    temporalAccumulationMat.SetName("SSIL_TemporalAccumulation_Material");

    Gfx::PipelineConfig::PipelineConfig_t config;
    config.color.blends.push_back({
        .blendEnable = true,
        .srcColorBlendFactor = Gfx::BlendFactor::One,
        .dstColorBlendFactor = Gfx::BlendFactor::One,
        .colorBlendOp = Gfx::BlendOp::Add,
        .srcAlphaBlendFactor = Gfx::BlendFactor::One,
        .dstAlphaBlendFactor = Gfx::BlendFactor::One,
        .alphaBlendOp = Gfx::BlendOp::Add,
    });
    config.depth.testEnable = false;
    config.depth.writeEnable = false;
    combineConfig = Gfx::PipelineConfig(config);

    geometryPass = std::make_unique<GeometryPass>();
    firstFilterPass = std::make_unique<BilateralFilterPass>();

    firstFilterPass->depthDiffSigma = 1.0f;
}

void SSIL::EnsureHistoryBuffers(int width, int height)
{
    if (historySize.x == width && historySize.y == height)
        return;

    historySize = {width, height};

    auto usage = Gfx::ImageUsage::Texture | Gfx::ImageUsage::Storage |
                 Gfx::ImageUsage::TransferDst | Gfx::ImageUsage::ColorAttachment;

    historySsil = GetGfxDriver()->CreateImage(
        Gfx::ImageDescription(width, height, Gfx::GfxFormat::R16G16B16A16_SFloat),
        usage
    );
    historySsil->SetName("SSIL_History");
    GetGfxDriver()->InitGfxImage(*historySsil, float4(0, 0, 0, 0));

    historyDepth = GetGfxDriver()->CreateImage(
        Gfx::ImageDescription(width, height, Gfx::GfxFormat::R32_SFloat),
        usage
    );
    historyDepth->SetName("SSIL_HistoryDepth");
    GetGfxDriver()->InitGfxImage(*historyDepth, float4(0, 0, 0, 0));

    historySmoothNormal = GetGfxDriver()->CreateImage(
        Gfx::ImageDescription(width, height, Gfx::GfxFormat::R32_UInt),
        usage
    );
    historySmoothNormal->SetName("SSIL_HistorySmoothNormal");
    GetGfxDriver()->InitGfxImage(*historySmoothNormal, float4(0, 0, 0, 0));
}

void SSIL::Execute(
    Gfx::CommandBuffer* cmd,
    const Gfx::ImageIdentifier& colorTex,
    const Gfx::ImageIdentifier& hizTex,
    const Gfx::ImageIdentifier& albedoTex,
    const Gfx::ImageIdentifier& motionVectorTex,
    const Gfx::ImageIdentifier& targetColor,
    RenderPipelineSetting* setting,
    RenderingData& renderingData
)
{
    (void)targetColor;

    if (!setting->ssil.enabled)
        return;

    firstFilterPass->depthDiffSigma = setting->ssil.filter1DepthDiffSigma;

    cmd->BeginLabel("SSIL", {0.1, 0.4, 0.6, 1.0});

    int width = (int(renderingData.screenSize.x) + 1) / 2;
    int height = (int(renderingData.screenSize.y) + 1) / 2;
    int fullWidth = int(renderingData.screenSize.x);
    int fullHeight = int(renderingData.screenSize.y);

    // 1. Generate full-res smooth normals
    {
        Gfx::RenderImageDescriptor smoothNormalDesc(fullWidth, fullHeight, Gfx::GfxFormat::R32_UInt);
        smoothNormalDesc.SetRandomWrite(true);
        cmd->AllocateAttachment(ssilSmoothNormal, smoothNormalDesc);

        smoothNormalMat.SetTexture("depthTex", GetGfxDriver()->GetImageFromRenderGraph(hizTex));
        smoothNormalMat.SetTexture("outNormalTex", GetGfxDriver()->GetImageFromRenderGraph(ssilSmoothNormal));
        smoothNormalMat.SetVector(
            "rtSize",
            glm::float4(fullWidth, fullHeight, 1.0f / fullWidth, 1.0f / fullHeight)
        );

        auto* smoothNormalProgram = smoothNormalMat.GetShaderProgram();
        cmd->BindResource(0, renderingData.globalResource);
        cmd->BindResource(smoothNormalMat.GetSet(Gfx::DescriptorSetSemantics::Material), smoothNormalMat.GetShaderResource());
        cmd->BindShaderProgram(smoothNormalProgram, smoothNormalProgram->GetDefaultShaderConfig());
        cmd->Dispatch((fullWidth + 7) / 8, (fullHeight + 7) / 8, 1);
    }

    // 2. Generate half-res packed geometry
    {
        Gfx::RenderImageDescriptor geometryDesc(width, height, Gfx::GfxFormat::R32G32B32A32_UInt);
        geometryDesc.SetRandomWrite(true);
        cmd->AllocateAttachment(ssilGeometry, geometryDesc);

        geometryPass->Execute(
            cmd,
            hizTex,
            ssilSmoothNormal,
            ssilGeometry,
            {width, height},
            {fullWidth, fullHeight}
        );
    }

    // 3. SSIL raw pass
    {
        Gfx::RenderImageDescriptor desc(width, height, Gfx::GfxFormat::R16G16B16A16_SFloat);
        desc.SetRandomWrite(true);
        cmd->AllocateAttachment(ssilRaw, desc);

        mat.SetTexture("geometryTex", GetGfxDriver()->GetImageFromRenderGraph(ssilGeometry));
        mat.SetTexture("colorTex", GetGfxDriver()->GetImageFromRenderGraph(colorTex));
        mat.SetTexture("albedoTex", GetGfxDriver()->GetImageFromRenderGraph(albedoTex));
        mat.SetTexture("blueNoise", renderingData.blueNoise.GetNoiseTexture());
        mat.SetTexture("outSsilTex", GetGfxDriver()->GetImageFromRenderGraph(ssilRaw));

        mat.SetVector("rtSize", glm::float4(width, height, 1.0f / width, 1.0f / height));
        mat.SetVector("fullResSize", glm::float4(fullWidth, fullHeight, 1.0f / fullWidth, 1.0f / fullHeight));
        mat.SetFloat("strength", setting->ssil.strength);
        mat.SetFloat("thickness", setting->ssil.thickness);
        mat.SetFloat("radius", setting->ssil.radius);
        mat.SetFloat("sliceCount", (float)setting->ssil.sliceCount);
        mat.SetFloat("sampleCount", (float)setting->ssil.sampleCount);
        mat.SetFloat("jitterScale", setting->ssil.jitterScale);
        mat.SetFloat("sampleJitterScale", setting->ssil.sampleJitterScale);
        mat.SetFloat("debug_ssilOutput", setting->ssil.debug_ssilOutput ? 1.0f : 0.0f);
        mat.SetVector("debugPoint", float4(setting->ssil.debugPoint, 0, 0));

        debugSSIL = setting->ssil.debug_ssilOutput;

        auto shaderProgram = mat.GetShaderProgram();
        cmd->BindResource(0, renderingData.globalResource);
        cmd->BindResource(mat.GetSet(Gfx::DescriptorSetSemantics::Material), mat.GetShaderResource());
        cmd->BindShaderProgram(shaderProgram, shaderProgram->GetDefaultShaderConfig());
        cmd->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
    }

    // 4. Bilateral filter
    {
        Gfx::RenderImageDescriptor desc(fullWidth, fullHeight, Gfx::GfxFormat::R16G16B16A16_SFloat);
        desc.SetRandomWrite(true);
        cmd->AllocateAttachment(ssilUpscaled, desc);
        firstFilterPass->Execute(
            cmd,
            ssilRaw,
            {width, height},
            {fullWidth, fullHeight},
            ssilGeometry,
            hizTex,
            ssilSmoothNormal,
            ssilUpscaled
        );
    }

    Gfx::RenderImageDescriptor ssilOutputDesc(fullWidth, fullHeight, Gfx::GfxFormat::R16G16B16A16_SFloat);
    ssilOutputDesc.SetRandomWrite(true);
    cmd->AllocateAttachment(ssil, ssilOutputDesc);
    if (!setting->ssil.temporalAccumulation)
    {
        cmd->Blit(ssilUpscaled, ssil);
        cmd->EndLabel();
        return;
    }

    EnsureHistoryBuffers(fullWidth, fullHeight);

    cmd->BeginLabel("SSIL_TemporalAccumulation", {0.12, 0.42, 0.62, 1.0});

    temporalAccumulationMat.SetTexture("currentSsilTex", GetGfxDriver()->GetImageFromRenderGraph(ssilUpscaled));
    temporalAccumulationMat.SetTexture("motionVectorTex", GetGfxDriver()->GetImageFromRenderGraph(motionVectorTex));
    temporalAccumulationMat.SetTexture("hierarchyDepthTex", GetGfxDriver()->GetImageFromRenderGraph(hizTex));
    temporalAccumulationMat.SetTexture("currentSmoothNormalTex", GetGfxDriver()->GetImageFromRenderGraph(ssilSmoothNormal));
    temporalAccumulationMat.SetTexture("historySsilTex", historySsil.get());
    temporalAccumulationMat.SetTexture("historyDepthTex", historyDepth.get());
    temporalAccumulationMat.SetTexture("historySmoothNormalTex", historySmoothNormal.get());
    temporalAccumulationMat.SetTexture("outSsilTex", GetGfxDriver()->GetImageFromRenderGraph(ssil));
    temporalAccumulationMat.SetVector(
        "rtSize",
        glm::float4(
            fullWidth,
            fullHeight,
            1.0f / fullWidth,
            1.0f / fullHeight
        )
    );
    temporalAccumulationMat.SetFloat("historyWeight", setting->ssil.temporalHistoryWeight);
    temporalAccumulationMat.SetFloat("depthTolerance", setting->ssil.temporalDepthTolerance);
    temporalAccumulationMat.SetFloat("normalThreshold", setting->ssil.temporalNormalThreshold);

    auto* temporalProgram = temporalAccumulationMat.GetShaderProgram();
    cmd->BindResource(0, renderingData.globalResource);
    cmd->BindResource(
        temporalAccumulationMat.GetSet(Gfx::DescriptorSetSemantics::Material),
        temporalAccumulationMat.GetShaderResource()
    );
    cmd->BindShaderProgram(temporalProgram, temporalProgram->GetDefaultShaderConfig());
    cmd->Dispatch((fullWidth + 7) / 8, (fullHeight + 7) / 8, 1);

    cmd->EndLabel();

    cmd->Blit(Gfx::ImageIdentifier(*GetGfxDriver()->GetImageFromRenderGraph(ssil)), Gfx::ImageIdentifier(*historySsil));
    cmd->Blit(Gfx::ImageIdentifier(*GetGfxDriver()->GetImageFromRenderGraph(hizTex)), Gfx::ImageIdentifier(*historyDepth));
    cmd->Blit(Gfx::ImageIdentifier(*GetGfxDriver()->GetImageFromRenderGraph(ssilSmoothNormal)), Gfx::ImageIdentifier(*historySmoothNormal));

    cmd->EndLabel();
}

bool SSIL::DebugBlit(Gfx::ImageIdentifier& dst)
{
    if (debugSSIL)
    {
        dst = ssil;
        return true;
    }
    return false;
}

void SSIL::ResetDebugState()
{
    debugSSIL = false;
}
} // namespace Rendering::Passes
