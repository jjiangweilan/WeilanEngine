#include "StaticMotionVectorPass.hpp"
#include "Engine/Runtime/System/Rendering/RenderingData.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipeline.hpp"

namespace Rendering::Passes
{
StaticMotionVectorPass::StaticMotionVectorPass()
{
    shader = ShaderLibrary::GetShader(Shaders::PostProcess_StaticMotionVector);
    material.SetShader(shader);

    Gfx::SubpassAttachment attachments[] = {
        {0, Gfx::AttachmentLoadOperation::Clear, Gfx::AttachmentStoreOperation::Store}
    };
    renderPass.SetSubpass(0, attachments);
}

void StaticMotionVectorPass::Execute(
    Gfx::CommandBuffer& cmd,
    const Gfx::ImageIdentifier& depth,
    const Gfx::RenderImageDescriptor& depthDesc,
    const RenderingData& renderingData
)
{
    cmd.BeginLabel("StaticMotionVector", {0.1f, 0.5f, 0.5f, 1.0f});

    Gfx::RenderImageDescriptor velocityDesc(
        depthDesc.GetWidth(),
        depthDesc.GetHeight(),
        Gfx::GfxFormat::R16G16_SFloat
    );
    cmd.AllocateAttachment(motionVector, velocityDesc);

    auto depthImage = GetGfxDriver()->GetImageFromRenderGraph(depth);
    material.SetTexture("depthTex", depthImage);

    renderPass.SetAttachment(0, motionVector);
    Gfx::ClearValue clear[] = {{0.0f, 0.0f, 0.0f, 0.0f}};
    cmd.BeginRenderPass(renderPass, clear);

    auto shaderProgram = material.GetShaderProgram();
    cmd.BindResource(material.GetSet(Gfx::DescriptorSetSemantics::Material), material.GetShaderResource());
    cmd.BindShaderProgram(shaderProgram, shaderProgram->GetDefaultShaderConfig());
    cmd.Draw(6, 1, 0, 0);

    cmd.EndRenderPass();
    cmd.EndLabel();

    // debugging toggle
    if (renderingData.renderPipelineSettings)
    {
        debugView = renderingData.renderPipelineSettings->debugDraw.motionVectors;
    }
}

bool StaticMotionVectorPass::DebugBlit(Gfx::ImageIdentifier& dst)
{
    if (debugView)
    {
        dst = motionVector;
        return true;
    }
    return false;
}

} // namespace Rendering::Passes
