#include "ColorGradingPass.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Rendering/Shader.hpp"
#include "Rendering/ShaderLibrary.hpp"

namespace Rendering::Passes
{
ColorGradingPass::ColorGradingPass()
{
    colorGradingShader = ShaderLibrary::GetShader(Shaders::ColorGrading);
    mat.SetShader(colorGradingShader);
}

void ColorGradingPass::OnInit(RenderingData* renderingData)
{
}

void ColorGradingPass::Execute(
    Gfx::CommandBuffer& cmd,
    Gfx::Image* mainColorInput,
    const glm::float2& rtSize
)
{
    auto shader = colorGradingShader->GetShaderProgram();
    
    Gfx::RenderImageDescriptor resultDesc(rtSize.x, rtSize.y, Gfx::GfxFormat::R8G8B8A8_SRGB);
    cmd.AllocateAttachment(colorGradingId, resultDesc);
    pass.SetAttachment(0, colorGradingId);
    mat.SetTexture("mainColor", mainColorInput);
    Gfx::ClearValue clears[] = {{0, 0, 0, 0}};
    cmd.BeginRenderPass(pass, clears);
    cmd.BindShaderProgram(shader, shader->GetDefaultShaderConfig());
    cmd.BindResource(0, mat.GetShaderResource());
    cmd.Draw(6, 1, 0, 0);
    cmd.EndRenderPass();
}

} // namespace Rendering::Passes
