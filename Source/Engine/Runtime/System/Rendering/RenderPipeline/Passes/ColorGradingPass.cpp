#include "ColorGradingPass.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Runtime/System/Rendering/Shader.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetDatabase.hpp"
#include "Engine/Runtime/Object/Texture/Texture.hpp"

namespace Rendering::Passes
{
ColorGradingPass::ColorGradingPass()
{
    colorGradingShader = ShaderLibrary::GetShader(Shaders::ColorGrading);
    mat.SetShader(colorGradingShader);
}

void ColorGradingPass::OnInit(RenderingData* renderingData)
{
    tonyMcMapfaceLUT = (Texture*)AssetDatabase::Singleton()->LoadAsset("_engine_internal/Textures/tony_mc_mapface.ktx");
}

void ColorGradingPass::Execute(
    Gfx::CommandBuffer& cmd,
    Gfx::Image* mainColorInput,
    const glm::float2& rtSize,
    const RenderPipelineSetting::PostProcess& settings,
    const RenderingData& renderingData
)
{
    auto shader = colorGradingShader->GetShaderProgram();
    
    Gfx::RenderImageDescriptor resultDesc(rtSize.x, rtSize.y, Gfx::GfxFormat::R8G8B8A8_SRGB);
    cmd.AllocateAttachment(colorGradingId, resultDesc);
    pass.SetAttachment(0, colorGradingId);
    mat.SetTexture("mainColor", mainColorInput);
    if (tonyMcMapfaceLUT)
    {
        mat.SetTexture("tonyMcMapfaceLUT", tonyMcMapfaceLUT->GetGfxImage());
    }

    ColorGradingInput input {
        .flags = {settings.tonemapMode, settings.hueValueSaturation ? 1u : 0u, 0u, 0u},
        .hsv = {settings.hue, settings.saturation, settings.value, 0.0f},
    };
    renderingData.pipelineAllocator->AllocateBuffer(colorGradingInputBuffer, sizeof(ColorGradingInput));
    colorGradingInputBuffer.Write(&input, sizeof(ColorGradingInput));
    mat.SetBuffer("settings", colorGradingInputBuffer.GetBuffer());

    Gfx::ClearValue clears[] = {{0, 0, 0, 0}};
    cmd.BeginRenderPass(pass, clears);
    cmd.BindShaderProgram(shader, shader->GetDefaultShaderConfig());
    cmd.BindResource(0, mat.GetShaderResource());

    cmd.Draw(6, 1, 0, 0);
    cmd.EndRenderPass();
}

} // namespace Rendering::Passes
