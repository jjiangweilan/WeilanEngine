#include "GBufferPass.hpp"
#include "Rendering/RenderingUtils.hpp"
namespace Rendering
{
GBufferPass::GBufferPass()
{
    gbufferPass = Gfx::RG::RenderPass(1, 5);
    Gfx::RG::SubpassAttachment lighting{0, Gfx::AttachmentLoadOperation::Clear, Gfx::AttachmentStoreOperation::Store};
    Gfx::RG::SubpassAttachment albedo{1};
    Gfx::RG::SubpassAttachment normal{2};
    Gfx::RG::SubpassAttachment property{3};
    Gfx::RG::SubpassAttachment depth{4};
    Gfx::RG::SubpassAttachment subpassAttachments[] = {lighting, albedo, normal, property};
    gbufferPass.SetSubpass(0, subpassAttachments, depth);
}

void GBufferPass::Execute(Gfx::CommandBuffer& cmd, RenderingData& renderingData, GBufferPass::Setting& setting)
{

    drawList = renderingData.drawList;

    int rtWidth = input.colorDesc.GetWidth();
    int rtHeight = input.colorDesc.GetHeight();
    albedoDesc.SetWidth(rtWidth);
    albedoDesc.SetHeight(rtHeight);
    albedoDesc.SetFormat(Gfx::ImageFormat::R8G8B8A8_SRGB);
    normalDesc.SetWidth(input.colorDesc.GetWidth());
    normalDesc.SetHeight(input.colorDesc.GetHeight());
    normalDesc.SetFormat(Gfx::ImageFormat::A2B10G10R10_UNorm);
    maskDesc.SetWidth(input.colorDesc.GetWidth());
    maskDesc.SetHeight(input.colorDesc.GetHeight());
    maskDesc.SetFormat(Gfx::ImageFormat::R8G8B8A8_UNorm);

    cmd.AllocateAttachment(output.albedo, albedoDesc);
    cmd.AllocateAttachment(output.normal, normalDesc);
    cmd.AllocateAttachment(output.mask, maskDesc);

    // set scissor and viewport
    Rect2D scissor = {{0, 0}, {static_cast<uint32_t>(rtWidth), static_cast<uint32_t>(rtHeight)}};
    cmd.SetScissor(0, 1, &scissor);
    Gfx::Viewport viewport{0, 0, static_cast<float>(rtWidth), static_cast<float>(rtHeight), 0, 1};
    cmd.SetViewport(viewport);

    // clear albedo to black
    // masks to 1 (mainly for ao)
    Gfx::ClearValue clears[] = {setting.clearValuesVal, {0, 0, 0, 0}, {0, 0, 0, 0}, {1.0f, 1.0f, 1.0f, 1.0f}, {1, 0}};
    gbufferPass.SetAttachment(0, input.color);
    gbufferPass.SetAttachment(1, output.albedo);
    gbufferPass.SetAttachment(2, output.normal);
    gbufferPass.SetAttachment(3, output.mask);
    gbufferPass.SetAttachment(4, input.depth);
    cmd.BeginRenderPass(gbufferPass, clears);

    // draw scene objects

    if (drawList)
    {
        for (int i = 0; i < drawList->alphaTestIndex; ++i)
        {
            auto& draw = drawList->at(i);
            auto shaderProgram = draw.material->GetShaderProgram("GBuffer");
            if (shaderProgram)
            {
                cmd.BindVertexBuffer(draw.vertexBufferBinding, 0);
                cmd.BindIndexBuffer(draw.indexBuffer, 0, draw.indexBufferType);
                cmd.BindResource(2, draw.shaderResource);
                cmd.BindShaderProgram(shaderProgram, shaderProgram->GetDefaultShaderConfig());
                cmd.SetPushConstant(shaderProgram, (void*)&draw.pushConstant);
                cmd.DrawIndexed(draw.indexCount, 1, 0, 0, 0);
            }
        }

        Shader::EnableFeature("_AlphaTest");
        for (int i = drawList->alphaTestIndex; i < drawList->transparentIndex; ++i)
        {
            auto& draw = drawList->at(i);
            auto shaderProgram = draw.material->GetShaderProgram("GBuffer");
            if (shaderProgram)
            {
                cmd.BindVertexBuffer(draw.vertexBufferBinding, 0);
                cmd.BindIndexBuffer(draw.indexBuffer, 0, draw.indexBufferType);
                cmd.BindResource(2, draw.shaderResource);
                cmd.BindShaderProgram(shaderProgram, shaderProgram->GetDefaultShaderConfig());
                cmd.SetPushConstant(shaderProgram, (void*)&draw.pushConstant);
                cmd.DrawIndexed(draw.indexCount, 1, 0, 0, 0);
            }
        }
        Shader::DisableFeature("_AlphaTest");

        RenderingUtils::DrawGraphics(cmd);
    }
    cmd.EndRenderPass();
}
} // namespace Rendering
