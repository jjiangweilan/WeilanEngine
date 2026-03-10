#include "SSIL.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Runtime/Object/Component/Camera.hpp"
#include "Engine/Runtime/System/Rendering/RenderingData.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"

namespace Rendering::Passes
{
SSIL::BilateralFilterPass::BilateralFilterPass()
{
    shader = ShaderLibrary::GetShader(ShaderLibrary::GetShaderName(Shaders::BilateralUpScale), {"_2x2_BILATERAL_UPSCALE", "_USE_BOX_FILTER_FOR_PIXEL_DISTANCE"});
    mat.SetShader(shader);
    mat.SetName("SSIL_BilateralFilter_Material");
}

void SSIL::BilateralFilterPass::Execute(
    Gfx::CommandBuffer* cmd,
    const Gfx::ImageIdentifier& sourceTex,
    glm::int2 sourceTexSize,
    const Gfx::ImageIdentifier& lowDepth,
    const Gfx::ImageIdentifier& highDepth,
    const Gfx::ImageIdentifier& destination,
    int lowDepthMipLevel
)
{
    if (sourceTexSize.x == 0 || sourceTexSize.y == 0)
        return;

    glm::int2 highResTexSize = sourceTexSize * 2;

    mat.SetTexture("lowColor", GetGfxDriver()->GetImageFromRenderGraph(sourceTex));
    mat.SetTexture("lowDepth", GetGfxDriver()->GetImageFromRenderGraph(lowDepth), Gfx::ImageViewOption(lowDepthMipLevel, 1, 0, 1, Gfx::ImageAspect::Color));
    mat.SetTexture("highDepth", GetGfxDriver()->GetImageFromRenderGraph(highDepth), Gfx::ImageViewOption(lowDepthMipLevel - 1, 1, 0, 1, Gfx::ImageAspect::Color));
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

    firstFilterPass = std::make_unique<BilateralFilterPass>();
    secondFilterPass = std::make_unique<BilateralFilterPass>();

    firstFilterPass->depthDiffSigma = 1.0f;
    secondFilterPass->depthDiffSigma = 1.0f;
}

void SSIL::Execute(
    Gfx::CommandBuffer* cmd,
    const Gfx::ImageIdentifier& colorTex,
    const Gfx::ImageIdentifier& hizTex,
    const Gfx::ImageIdentifier& albedoTex,
    const Gfx::ImageIdentifier& normalTex,
    const Gfx::ImageIdentifier& targetColor,
    RenderPipelineSetting* setting,
    RenderingData& renderingData
)
{
    if (!setting->ssil.enabled)
        return;

    firstFilterPass->depthDiffSigma = setting->ssil.filter1DepthDiffSigma;
    secondFilterPass->depthDiffSigma = setting->ssil.filter2DepthDiffSigma;

    cmd->BeginLabel("SSIL", {0.1, 0.4, 0.6, 1.0});

    int width = renderingData.screenSize.x / 4;
    int height = renderingData.screenSize.y / 4;

    Gfx::RenderImageDescriptor desc(width, height, Gfx::GfxFormat::R16G16B16A16_SFloat);
    desc.SetRandomWrite(true);
    cmd->AllocateAttachment(ssilRaw, desc);

    mat.SetTexture("depthTex", GetGfxDriver()->GetImageFromRenderGraph(hizTex));
    mat.SetTexture("albedoTex", GetGfxDriver()->GetImageFromRenderGraph(albedoTex));
    mat.SetTexture("normalTex", GetGfxDriver()->GetImageFromRenderGraph(normalTex));
    mat.SetTexture("colorTex", GetGfxDriver()->GetImageFromRenderGraph(colorTex));
    mat.SetTexture("ignNoise", renderingData.interleavedGradientNoise.GetNoiseTexture());
    mat.SetTexture("outSsilTex", GetGfxDriver()->GetImageFromRenderGraph(ssilRaw));

    mat.SetVector("rtSize", glm::float4(width, height, 1.0f / width, 1.0f / height));
    mat.SetFloat("strength", setting->ssil.strength);
    mat.SetFloat("thickness", setting->ssil.thickness);
    mat.SetFloat("radius", setting->ssil.radius);
    mat.SetFloat("sliceCount", (float)setting->ssil.sliceCount);
    mat.SetFloat("sampleCount", (float)setting->ssil.sampleCount);
    mat.SetFloat("jitterScale", setting->ssil.jitterScale);
    mat.SetFloat("debug_ssilOutput", setting->ssil.debug_ssilOutput ? 1.0f : 0.0f);
    mat.SetVector("debugPoint", float4(setting->ssil.debugPoint, 0, 0));

    debugSSIL = setting->ssil.debug_ssilOutput;

    auto shaderProgram = mat.GetShaderProgram();
    cmd->BindResource(mat.GetSet(Gfx::DescriptorSetSemantics::Material), mat.GetShaderResource());
    cmd->BindShaderProgram(shaderProgram, shaderProgram->GetDefaultShaderConfig());
    cmd->Dispatch((width + 7) / 8, (height + 7) / 8, 1);

    desc.SetRandomWrite(true);
    desc.SetWidth(width * 2);
    desc.SetHeight(height * 2);
    cmd->AllocateAttachment(firstFilterPassOutput, desc);
    firstFilterPass->Execute(
        cmd,
        ssilRaw,
        {width, height},
        hizTex,
        hizTex,
        firstFilterPassOutput,
        2
    );

    desc.SetWidth(renderingData.screenSize.x);
    desc.SetHeight(renderingData.screenSize.y);
    cmd->AllocateAttachment(ssil, desc);
    secondFilterPass->Execute(
        cmd,
        firstFilterPassOutput,
        {width * 2, height * 2},
        hizTex,
        hizTex,
        ssil,
        1
    );

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
} // namespace Rendering::Passes
