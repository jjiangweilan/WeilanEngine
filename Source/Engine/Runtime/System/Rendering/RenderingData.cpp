#include "RenderingData.hpp"

namespace Rendering
{
Gfx::Image* InterleavedGradientNoise::GetNoiseTexture() const
{
    if (interleavedGradientNoise == nullptr)
    {
        Gfx::ImageDescription interleavedGradientNoiseDesc(32, 32, 1, Gfx::GfxFormat::R8_UNorm);
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
} // namespace Rendering
