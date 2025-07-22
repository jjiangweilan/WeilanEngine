#include "SSAO.hpp"

namespace Rendering::Passes
{
SSAO::SSAO()
{
    ssaoShader = ShaderLibrary::GetShader(ShaderLibrary::PostProcess_SSAO);
    mat.SetShader(ssaoShader);
}

void SSAO::Execute(
    Gfx::CommandBuffer* cmd,
    const Gfx::RG::ImageIdentifier& halfResDepth,
    const Gfx::RG::ImageIdentifier& fullResDepth,
    const Gfx::RG::ImageDescription& fullResDepthDesc,
    RenderPipelineSetting* setting
)
{
    mat.SetFloat("strength", setting->ssao.strength);
    mat.SetFloat("scaling", setting->ssao.scaling);
    mat.SetFloat("falloff", setting->ssao.falloff);
    mat.SetFloat("bias", setting->ssao.bias);

    float2 rtSize = {fullResDepthDesc.GetWidth() / 2, fullResDepthDesc.GetHeight() / 2};
    mat.SetVector("rtSize", glm::float4(rtSize.x, rtSize.y, 1.0f / rtSize.x, 1.0f / rtSize.y));
    mat.SetTexture("depthTex", GetGfxDriver()->GetImageFromRenderGraph(halfResDepth));

    Gfx::ClearValue clears[] = {{1.0f, 1.0f, 1.0f, 1.0f}};
    Gfx::RG::ImageDescription desc(rtSize.x, rtSize.y, Gfx::GfxFormat::R32_SFloat);
    Gfx::RG::ImageDescription fullDesc(
        fullResDepthDesc.GetWidth(),
        fullResDepthDesc.GetHeight(),
        Gfx::GfxFormat::R32_SFloat
    );

    cmd->AllocateAttachment(ssaoDownSampled, desc);
    cmd->AllocateAttachment(ssao, fullDesc);

    pass.SetAttachment(0, ssaoDownSampled);
    cmd->BeginLabel("SSAO", {0.3, 0.1, 0.5, 1.0});
    cmd->BeginRenderPass(pass, clears);
    if (setting->ssao.enabled)
    {
        auto shaderProgram = mat.GetShaderProgram();
        cmd->BindResource(mat.GetSet(Gfx::DescriptorSetSemantics::Material), mat.GetShaderResource());
        cmd->BindShaderProgram(shaderProgram, shaderProgram->GetDefaultShaderConfig());
        cmd->Draw(6, 1, 0, 0);
    }
    cmd->EndRenderPass();
    cmd->EndLabel(); // SSAO

    // upscale
    DepthAwareBilateralUpsampler::GPUInput upscalerInput{
        .highResTexSize = {fullDesc.GetWidth(), fullDesc.GetHeight()},
        .kernelSize = 1,
        .integerCoordSigma = setting->ssao.bilateralUpScaleIntegerCoordSigma,
        .depthDiffSigma = setting->ssao.bilateralUpScaleDepthDiffSigma
    };
    upscaler.Setup(ssaoDownSampled, halfResDepth, fullResDepth, ssao, upscalerInput);
    upscaler.Execute(*cmd);
}

} // namespace Rendering::Passes
