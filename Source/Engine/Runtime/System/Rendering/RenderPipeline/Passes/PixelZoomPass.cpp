#include "PixelZoomPass.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"

namespace Rendering::Passes
{
PixelZoomPass::PixelZoomPass()
{
    shader = ShaderLibrary::GetShader(Shaders::PostProcess_PixelZoom);
    resource = GetGfxDriver()->CreateShaderResource();

    Gfx::SubpassAttachment attachmentDesc{0, Gfx::AttachmentLoadOperation::DontCare, Gfx::AttachmentStoreOperation::Store};
    Gfx::SubpassAttachment attachments[] = {attachmentDesc};
    pass.SetSubpass(0, attachments);
}

void PixelZoomPass::Execute(
    Gfx::CommandBuffer& cmd,
    const Gfx::ImageIdentifier& src,
    const Gfx::ImageIdentifier& dst,
    glm::vec2 mousePos,
    glm::vec2 screenSize
)
{
    pass.SetAttachment(0, dst);
    resource->SetImage("input", GetGfxDriver()->GetImageFromRenderGraph(src));

    struct PushConstant
    {
        glm::vec2 mousePos;
        glm::vec2 screenSize;
        float zoomFactor;
        float zoomWindowSize;
    } pc = {mousePos, screenSize, PixelZoomPass::zoomFactor, PixelZoomPass::zoomWindowSize};

    Gfx::ClearValue clears[] = {{0, 0, 0, 0}};
    Gfx::RenderAttachment attachments[] = {
        {dst, Gfx::AttachmentLoadOperation::DontCare, Gfx::AttachmentStoreOperation::Store}
    };
    cmd.BeginRenderPass(attachments, clears);
    cmd.SetPushConstant(shader->GetShaderProgram(), &pc);
    cmd.BindResource(0, resource.get());
    cmd.BindShaderProgram(shader->GetShaderProgram(), shader->GetShaderProgram()->GetDefaultShaderConfig());
    cmd.Draw(6, 1, 0, 0);
    cmd.EndRenderPass();
}

} // namespace Rendering::Passes
