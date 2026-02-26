#include "SSAO.hpp"
#include "Engine/Runtime/Object/Component/Camera.hpp"
#include "Engine/Runtime/System/Rendering/RenderingData.hpp"

namespace Rendering::Passes
{
SSAO::SSAO()
{
    ssaoShader = ShaderLibrary::GetShader(Shaders::PostProcess_SSAO);
    mat.SetShader(ssaoShader);
}

void SSAO::Execute(
    Gfx::CommandBuffer* cmd,
    const Gfx::ImageIdentifier& halfResDepth,
    const Gfx::ImageIdentifier& fullResDepth,
    const Gfx::RenderImageDescriptor& fullResDepthDesc,
    RenderPipelineSetting* setting,
    RenderingData& renderingData
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
    Gfx::RenderImageDescriptor desc(rtSize.x, rtSize.y, Gfx::GfxFormat::R32_SFloat);
    Gfx::RenderImageDescriptor fullDesc(
        fullResDepthDesc.GetWidth(),
        fullResDepthDesc.GetHeight(),
        Gfx::GfxFormat::R32_SFloat
    );
    fullDesc.SetRandomWrite(true);

    mat.SetTexture("ignNoise", renderingData.interleavedGradientNoise.GetNoiseTexture());
    // Setup debug if needed
    if (setting->ssao.debug_showNormal)
    {

        Gfx::RenderImageDescriptor desc(rtSize.x, rtSize.y, Gfx::GfxFormat::R32G32B32A32_SFloat);
        desc.SetRandomWrite(true);

        cmd->AllocateAttachment(debugImage, desc);
        mat.EnableFeature("DEBUG_show_normal_reconstruction");
        mat.SetTexture("debugTex", GetGfxDriver()->GetImageFromRenderGraph(debugImage));
        debugNormal = true;
    }
    else if (setting->ssao.debug_ssaoOutput)
    {
        debugFinalSSAO = true;
    }
    else
    {
        debugFinalSSAO = false;
        debugNormal = false;
        mat.DisableFeature("DEBUG_show_normal_reconstruction");
        mat.SetTexture("debugTex", nullptr);
    }

    // Allocate resources
    // TODO: when ssao is not needed we can return a small white built in texture to save these allocations
    if (useUpscaler)
        cmd->AllocateAttachment(ssaoDownSampled, desc);
    cmd->AllocateAttachment(ssao, fullDesc);
    Gfx::ImageIdentifier& ssaoSrc = useUpscaler ? ssaoDownSampled : ssao;

    cmd->BeginLabel("SSAO", {0.3, 0.1, 0.5, 1.0});
    if (setting->ssao.enabled)
    {
        // Setup pass data
        mat.SetFloat("strength", setting->ssao.strength);
        mat.SetFloat("scaling", setting->ssao.scaling);
        mat.SetFloat("falloff", setting->ssao.falloff);
        mat.SetFloat("bias", setting->ssao.bias);

        float s_bar = fullResDepthDesc.GetWidth() / (2 * renderingData.mainCamera->GetProjectionRight()) * renderingData.mainCamera->GetNear();
        mat.SetFloat("oneMeterPixelSize", s_bar);
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

bool SSAO::DebugBlit(Gfx::ImageIdentifier& dst)
{
    if (debugNormal)
    {
        dst = debugImage;
        return true;
    }
    else if (debugFinalSSAO)
    {
        dst = ssao;
        return true;
    }

    return false;
}

} // namespace Rendering::Passes
