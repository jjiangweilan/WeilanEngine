#include "TAAPass.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"

namespace Rendering::Passes
{
namespace
{
float RadicalInverse(uint32_t index, uint32_t base)
{
    float result = 0.0f;
    float inverseBase = 1.0f / static_cast<float>(base);
    float fraction = inverseBase;
    while (index > 0)
    {
        result += static_cast<float>(index % base) * fraction;
        index /= base;
        fraction *= inverseBase;
    }
    return result;
}
} // namespace

TAAPass::TAAPass()
{
    shader = ShaderLibrary::GetShader(Shaders::PostProcess_TAA);
    material.SetShader(shader);
    material.SetName("TAA Material");
}

glm::vec2 TAAPass::GetProjectionJitterNdc(
    uint32_t frameIndex,
    const glm::vec2& renderSize,
    float jitterScale
)
{
    uint32_t sampleIndex = frameIndex % 8u + 1u;
    glm::vec2 sample = {
        RadicalInverse(sampleIndex, 2u) - 0.5f,
        RadicalInverse(sampleIndex, 3u) - 0.5f
    };
    return sample * jitterScale * 2.0f / renderSize;
}

void TAAPass::EnsureHistoryBuffers(int width, int height)
{
    if (historySize.x == width && historySize.y == height)
        return;

    historySize = {width, height};
    historyValid = false;

    auto usage = Gfx::ImageUsage::Texture | Gfx::ImageUsage::Storage |
                 Gfx::ImageUsage::TransferDst | Gfx::ImageUsage::ColorAttachment;

    historyColor = GetGfxDriver()->CreateImage(
        Gfx::ImageDescription(width, height, Gfx::GfxFormat::R16G16B16A16_SFloat),
        usage
    );
    historyColor->SetName("TAA History Color");
    GetGfxDriver()->InitGfxImage(*historyColor, glm::vec4(0.0f));

    historyDepth = GetGfxDriver()->CreateImage(
        Gfx::ImageDescription(width, height, Gfx::GfxFormat::R32_SFloat),
        usage
    );
    historyDepth->SetName("TAA History Depth");
    GetGfxDriver()->InitGfxImage(*historyDepth, glm::vec4(0.0f));
}

void TAAPass::Execute(
    Gfx::CommandBuffer& cmd,
    const Gfx::ImageIdentifier& currentColor,
    const Gfx::ImageIdentifier& currentDepth,
    const Gfx::ImageIdentifier& motionVectors,
    const Gfx::RenderImageDescriptor& colorDesc,
    const RenderPipelineSetting::TAA& settings,
    const RenderingData& renderingData
)
{
    int width = static_cast<int>(colorDesc.GetWidth());
    int height = static_cast<int>(colorDesc.GetHeight());
    EnsureHistoryBuffers(width, height);

    if (historyScene != renderingData.scene || historyCamera != renderingData.mainCamera)
    {
        historyScene = renderingData.scene;
        historyCamera = renderingData.mainCamera;
        historyValid = false;
    }

    Gfx::RenderImageDescriptor outputDesc(width, height, Gfx::GfxFormat::R16G16B16A16_SFloat, true);
    cmd.AllocateAttachment(output, outputDesc);
    cmd.AllocateAttachment(historyOutput, outputDesc);

    material.SetTexture("currentColorTex", GetGfxDriver()->GetImageFromRenderGraph(currentColor));
    material.SetTexture("motionVectorTex", GetGfxDriver()->GetImageFromRenderGraph(motionVectors));
    material.SetTexture("currentDepthTex", GetGfxDriver()->GetImageFromRenderGraph(currentDepth));
    material.SetTexture("historyColorTex", historyColor.get());
    material.SetTexture("historyDepthTex", historyDepth.get());
    material.SetTexture("outColorTex", GetGfxDriver()->GetImageFromRenderGraph(output));
    material.SetTexture("outHistoryTex", GetGfxDriver()->GetImageFromRenderGraph(historyOutput));
    material.SetVector(
        "rtSize",
        glm::vec4(width, height, 1.0f / static_cast<float>(width), 1.0f / static_cast<float>(height))
    );
    material.SetFloat("historyWeight", settings.historyWeight);
    material.SetFloat("varianceClipGamma", settings.varianceClipGamma);
    material.SetFloat("sharpness", settings.sharpness);
    material.SetFloat("historyValid", historyValid ? 1.0f : 0.0f);

    cmd.BeginLabel("Temporal Anti Aliasing", {0.22f, 0.48f, 0.82f, 1.0f});
    auto* program = material.GetShaderProgram();
    cmd.BindResource(0, renderingData.globalResource);
    cmd.BindResource(material.GetSet(Gfx::DescriptorSetSemantics::Material), material.GetShaderResource());
    cmd.BindShaderProgram(program, program->GetDefaultShaderConfig());
    cmd.Dispatch((width + 7) / 8, (height + 7) / 8, 1);
    cmd.EndLabel();

    cmd.Blit(Gfx::ImageIdentifier(*GetGfxDriver()->GetImageFromRenderGraph(historyOutput)), Gfx::ImageIdentifier(*historyColor));
    cmd.Blit(Gfx::ImageIdentifier(*GetGfxDriver()->GetImageFromRenderGraph(currentDepth)), Gfx::ImageIdentifier(*historyDepth));
    historyValid = true;
}

void TAAPass::ResetHistory()
{
    historyValid = false;
    historyScene = nullptr;
    historyCamera = nullptr;
}
} // namespace Rendering::Passes
