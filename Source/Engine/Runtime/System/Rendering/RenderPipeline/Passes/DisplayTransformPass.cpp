#include "DisplayTransformPass.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Runtime/System/Rendering/Shader.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetDatabase.hpp"
#include "Engine/Runtime/Object/Texture/Texture.hpp"
#include <spdlog/spdlog.h>

namespace Rendering::Passes
{
DisplayTransformPass::DisplayTransformPass()
{
    displayTransformShader = ShaderLibrary::GetShader(Shaders::DisplayTransform);
    mat.SetShader(displayTransformShader);
}

void DisplayTransformPass::OnInit(RenderingData* renderingData)
{
    tonyMcMapfaceLUT = (Texture*)AssetDatabase::Singleton()->LoadAsset("_engine_internal/Textures/tony_mc_mapface.ktx2");
    if (!tonyMcMapfaceLUT)
    {
        SPDLOG_ERROR("Tony McMapface LUT could not be loaded; using ACES for Tony requests");
    }
    else
    {
        const auto& desc = tonyMcMapfaceLUT->GetDescription().img;
        if (desc.width != 48 || desc.height != 48 || desc.depth != 48 || desc.mipLevels != 1 ||
            desc.format != Gfx::GfxFormat::E5B9G9R9_UFloat_Pack32)
        {
            SPDLOG_ERROR(
                "Tony McMapface LUT must be a single-level 48x48x48 E5B9G9R9_UFloat_Pack32 texture; using ACES"
            );
            tonyMcMapfaceLUT = nullptr;
        }
    }
}

void DisplayTransformPass::Execute(
    Gfx::CommandBuffer& cmd,
    Gfx::Image* mainColorInput,
    const glm::float2& rtSize,
    const RenderPipelineSetting::PostProcess& settings,
    const RenderingData& renderingData
)
{
    auto shader = displayTransformShader->GetShaderProgram();
    
    Gfx::RenderImageDescriptor resultDesc(rtSize.x, rtSize.y, Gfx::GfxFormat::R8G8B8A8_SRGB);
    cmd.AllocateAttachment(outputId, resultDesc);
    pass.SetAttachment(0, outputId);
    mat.SetTexture("mainColor", mainColorInput);
    if (tonyMcMapfaceLUT)
    {
        mat.SetTexture("tonyMcMapfaceLUT", tonyMcMapfaceLUT->GetGfxImage());
    }

    const uint32_t requestedMode = settings.colorGrading
        ? settings.tonemapMode
        : static_cast<uint32_t>(RenderPipelineSetting::TonemapMode::None);
    const uint32_t mode = requestedMode == static_cast<uint32_t>(RenderPipelineSetting::TonemapMode::TonyMcMapface) && !tonyMcMapfaceLUT
        ? static_cast<uint32_t>(RenderPipelineSetting::TonemapMode::ACES)
        : requestedMode;
    DisplayTransformInput input {
        .flags = {mode, settings.colorGrading && settings.hueValueSaturation ? 1u : 0u, 0u, 0u},
        .hsv = {settings.hue, settings.saturation, settings.value, 0.0f},
        .exposure = {settings.exposureEV, 0.0f, 0.0f, 0.0f},
    };
    renderingData.pipelineAllocator->AllocateBuffer(inputBuffer, sizeof(DisplayTransformInput));
    inputBuffer.Write(&input, sizeof(DisplayTransformInput));
    mat.SetBuffer("settings", inputBuffer.GetBuffer());

    Gfx::ClearValue clears[] = {{0, 0, 0, 0}};
    cmd.BeginRenderPass(pass, clears);
    cmd.BindShaderProgram(shader, shader->GetDefaultShaderConfig());
    cmd.BindResource(0, mat.GetShaderResource());

    cmd.Draw(6, 1, 0, 0);
    cmd.EndRenderPass();
}

} // namespace Rendering::Passes
