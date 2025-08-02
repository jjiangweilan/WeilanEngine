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
    // Prepare data
    bool useUpscaler = setting->ssao.enabled && setting->ssao.enableUpscaler;
    float scale = useUpscaler ? 0.5f : 1.0f;
    int2 rtSize = {fullResDepthDesc.GetWidth() * scale, fullResDepthDesc.GetHeight() * scale};
    auto halfResDepthImage = GetGfxDriver()->GetImageFromRenderGraph(halfResDepth);
    auto fullResDepthImage = GetGfxDriver()->GetImageFromRenderGraph(fullResDepth);
    auto depthTex = useUpscaler ? halfResDepthImage : fullResDepthImage;

    // Create GPU resources
    Gfx::ClearValue clears[] = {{1.0f, 1.0f, 1.0f, 1.0f}};
    Gfx::RG::ImageDescription desc(rtSize.x, rtSize.y, Gfx::GfxFormat::R32_SFloat);
    Gfx::RG::ImageDescription fullDesc(
        fullResDepthDesc.GetWidth(),
        fullResDepthDesc.GetHeight(),
        Gfx::GfxFormat::R32_SFloat
    );
    fullDesc.SetRandomWrite(true);

    // Allocate resources
    // TODO: when ssao is not needed we can return a small white built in texture to save these allocations
    if (useUpscaler)
        cmd->AllocateAttachment(ssaoDownSampled, desc);
    cmd->AllocateAttachment(ssao, fullDesc);
    Gfx::RG::ImageIdentifier& ssaoSrc = useUpscaler ? ssaoDownSampled : ssao;

    cmd->BeginLabel("SSAO", {0.3, 0.1, 0.5, 1.0});
    if (setting->ssao.enabled)
    {
        // Setup pass data
        mat.SetFloat("strength", setting->ssao.strength);
        mat.SetFloat("scaling", setting->ssao.scaling);
        mat.SetFloat("falloff", setting->ssao.falloff);
        mat.SetFloat("bias", setting->ssao.bias);
        mat.SetVector("rtSize", glm::float4(rtSize.x, rtSize.y, 1.0f / rtSize.x, 1.0f / rtSize.y));
        mat.SetTexture("depthTex", depthTex);

        // Dispatch SSAO
        pass.SetAttachment(0, ssaoSrc);
        cmd->BeginLabel("SSAO Gather", {0.215, 0.567, 0.763, 1.0});
        cmd->BeginRenderPass(pass, clears);
        auto shaderProgram = mat.GetShaderProgram();
        cmd->BindResource(mat.GetSet(Gfx::DescriptorSetSemantics::Material), mat.GetShaderResource());
        cmd->BindShaderProgram(shaderProgram, shaderProgram->GetDefaultShaderConfig());
        cmd->Draw(6, 1, 0, 0);
        cmd->EndRenderPass();
        cmd->EndLabel();

        // upscale
        if (setting->ssao.enableUpscaler)
        {
            cmd->BeginLabel("Upsample", {0.215, 0.567, 0.763, 1.0});
            DepthAwareBilateralUpsampler::GPUInput upscalerInput{
                .highResTexSize = {fullDesc.GetWidth(), fullDesc.GetHeight()},
                .kernelSize = setting->ssao.bilateralUpScaleKernelSize,
                .integerCoordSigma = setting->ssao.bilateralUpScaleIntegerCoordSigma,
                .depthDiffSigma = setting->ssao.bilateralUpScaleDepthDiffSigma
            };
            upscaler.Setup(ssaoDownSampled, halfResDepth, fullResDepth, ssao, upscalerInput);
            upscaler.Execute(*cmd);
            cmd->EndLabel();
        }
    }
    else
    {
        // We need to clear ssao if it's not needed
        pass.SetAttachment(0, ssaoSrc);
        cmd->BeginRenderPass(pass, clears);
        cmd->EndRenderPass();
    }

    cmd->EndLabel(); // SSAO

    result = &ssao;
}

Gfx::Image& SSAO::GetDebugImage()
{
    return *debugImage;
}

bool SSAO::DebugBlit(Gfx::RG::ImageIdentifier& src)
{
    return false;
}

} // namespace Rendering::Passes
