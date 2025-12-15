#include "FXAAPass.hpp"
#include "Driver/GfxDriver/GfxDriver.hpp"
#include "Runtime/System/Rendering/ShaderLibrary.hpp"

namespace Rendering::Passes
{
FXAAPass::FXAAPass()
{
    shader = ShaderLibrary::GetShader(Shaders::FXAA);
    resource = GetGfxDriver()->CreateShaderResource();

    Gfx::SubpassAttachment attachmentDesc{0, Gfx::AttachmentLoadOperation::Load, Gfx::AttachmentStoreOperation::Store};
    Gfx::SubpassAttachment attachments[] = {attachmentDesc};
    pass.SetSubpass(0, attachments);
}

void FXAAPass::OnInit(RenderingData* renderingData)
{
}

void FXAAPass::Execute(
    Gfx::CommandBuffer& cmd,
    const glm::float4& sourceSize,
    const Gfx::ImageIdentifier& src,
    const Gfx::ImageIdentifier& dst
)
{
    pass.SetAttachment(0, dst);
    resource->SetImage("source", GetGfxDriver()->GetImageFromRenderGraph(src));
    Gfx::ClearValue clears[] = {{0, 0, 0, 0}};
    Gfx::RenderAttachment attachments[] = {
        {dst, Gfx::AttachmentLoadOperation::Clear, Gfx::AttachmentStoreOperation::Store}
    };
    cmd.BeginRenderPass(attachments, clears);
    cmd.SetPushConstant(shader->GetShaderProgram(), (void*)&sourceSize[0]);
    cmd.BindResource(0, resource.get());
    cmd.BindShaderProgram(shader->GetShaderProgram(), shader->GetShaderProgram()->GetDefaultShaderConfig());
    cmd.Draw(6, 1, 0, 0);
    cmd.EndRenderPass();
}

} // namespace Rendering::Passes
