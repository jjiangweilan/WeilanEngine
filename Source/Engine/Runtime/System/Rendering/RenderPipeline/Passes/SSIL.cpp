#include "SSIL.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Runtime/Object/Component/Camera.hpp"
#include "Engine/Runtime/System/Rendering/RenderingData.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"

namespace Rendering::Passes
{
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
}

void SSIL::Execute(
    Gfx::CommandBuffer* cmd,
    const Gfx::ImageIdentifier& colorTex,
    const Gfx::ImageIdentifier& depthTex,
    const Gfx::ImageIdentifier& albedoTex,
    const Gfx::ImageIdentifier& normalTex,
    const Gfx::ImageIdentifier& targetColor,
    RenderPipelineSetting* setting,
    RenderingData& renderingData
)
{
    if (!setting->ssil.enabled)
        return;

    cmd->BeginLabel("SSIL", {0.1, 0.4, 0.6, 1.0});

    int width = renderingData.screenSize.x;
    int height = renderingData.screenSize.y;

    Gfx::RenderImageDescriptor desc(width, height, Gfx::GfxFormat::R16G16B16A16_SFloat);
    cmd->AllocateAttachment(ssil, desc);

    mat.SetTexture("depthTex", GetGfxDriver()->GetImageFromRenderGraph(depthTex));
    mat.SetTexture("albedoTex", GetGfxDriver()->GetImageFromRenderGraph(albedoTex));
    mat.SetTexture("normalTex", GetGfxDriver()->GetImageFromRenderGraph(normalTex));
    mat.SetTexture("colorTex", GetGfxDriver()->GetImageFromRenderGraph(colorTex));
    mat.SetTexture("ignNoise", renderingData.interleavedGradientNoise.GetNoiseTexture());

    mat.SetVector("rtSize", glm::float4(width, height, 1.0f / width, 1.0f / height));
    mat.SetFloat("strength", setting->ssil.strength);
    mat.SetFloat("thickness", setting->ssil.thickness);
    mat.SetFloat("radius", setting->ssil.radius);
    mat.SetFloat("sliceCount", (float)setting->ssil.sliceCount);
    mat.SetFloat("sampleCount", (float)setting->ssil.sampleCount);
    mat.SetFloat("debug_ssilOutput", setting->ssil.debug_ssilOutput ? 1.0f : 0.0f);
    mat.SetVector("debugPoint", float4(setting->ssil.debugPoint, 0, 0));

    debugSSIL = setting->ssil.debug_ssilOutput;

    Gfx::RenderAttachment attachments[] = {
        {ssil, Gfx::AttachmentLoadOperation::Clear}
    };
    Gfx::ClearValue clears[] = {{0, 0, 0, 0}};
    cmd->BeginRenderPass(attachments, clears);

    auto shaderProgram = mat.GetShaderProgram();
    cmd->BindResource(mat.GetSet(Gfx::DescriptorSetSemantics::Material), mat.GetShaderResource());
    cmd->BindShaderProgram(shaderProgram, shaderProgram->GetDefaultShaderConfig());
    cmd->Draw(6, 1, 0, 0);

    cmd->EndRenderPass();

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
