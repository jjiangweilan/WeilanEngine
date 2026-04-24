#include "RenderingData.hpp"

namespace Rendering
{
Gfx::Image* InterleavedGradientNoise::GetNoiseTexture() const
{
    if (interleavedGradientNoise == nullptr)
    {
        Gfx::ImageDescription interleavedGradientNoiseDesc(128, 128, 1, Gfx::GfxFormat::R8G8_UNorm);
        interleavedGradientNoise = GetGfxDriver()->CreateImage(
            interleavedGradientNoiseDesc,
            Gfx::ImageUsage::Texture | Gfx::ImageUsage::Storage
        );
        auto interleavedGradientNoiseShader = ShaderLibrary::GetShader(Shaders::InterleavedGradientNoise);
        interleavedGradientNoiseMat.SetShader(interleavedGradientNoiseShader);
        interleavedGradientNoiseMat.SetTexture("tex", interleavedGradientNoise.get());
        auto cmd = GetGfxDriver()->CreateCommandBuffer();
        cmd->BindResource(0, interleavedGradientNoiseMat.GetShaderResource());
        cmd->BindShaderProgram(
            interleavedGradientNoiseShader->GetShaderProgram(),
            interleavedGradientNoiseShader->GetShaderProgram()->GetDefaultShaderConfig()
        );
        cmd->Dispatch((interleavedGradientNoiseDesc.width + 7) / 8, (interleavedGradientNoiseDesc.height + 7) / 8, 1);
        GetGfxDriver()->ExecuteCommandBuffer(*cmd);
    }

    return interleavedGradientNoise.get();
}

Gfx::Image* BlueNoise::GetNoiseTexture() const
{
    if (blueNoise == nullptr)
    {
        Gfx::ImageDescription blueNoiseDesc(128, 128, 1, Gfx::GfxFormat::R8G8_UNorm);
        blueNoise = GetGfxDriver()->CreateImage(
            blueNoiseDesc,
            Gfx::ImageUsage::Texture | Gfx::ImageUsage::Storage
        );
        auto blueNoiseShader = ShaderLibrary::GetShader(Shaders::BlueNoise);
        blueNoiseMat.SetShader(blueNoiseShader);
        blueNoiseMat.SetTexture("tex", blueNoise.get());
        auto cmd = GetGfxDriver()->CreateCommandBuffer();
        cmd->BindResource(0, blueNoiseMat.GetShaderResource());
        cmd->BindShaderProgram(
            blueNoiseShader->GetShaderProgram(),
            blueNoiseShader->GetShaderProgram()->GetDefaultShaderConfig()
        );
        cmd->Dispatch((blueNoiseDesc.width + 7) / 8, (blueNoiseDesc.height + 7) / 8, 1);
        GetGfxDriver()->ExecuteCommandBuffer(*cmd);
    }

    return blueNoise.get();
}
} // namespace Rendering
