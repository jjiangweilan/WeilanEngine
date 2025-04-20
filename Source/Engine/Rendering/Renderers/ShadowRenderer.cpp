#include "ShadowRenderer.hpp"

namespace Rendering
{
void ShadowRenderer::Init()
{
    pass = Gfx::RG::RenderPass(1, 1);
    pass.SetSubpass(
        0,
        {},
        Gfx::RG::SubpassAttachment{ 0, Gfx::AttachmentLoadOperation::Clear, Gfx::AttachmentStoreOperation::Store }
    );
    pass.SetName("ShadowMap pass");
    shadowMapShader = ShaderLibrary::GetShader(ShaderLibrary::ShadowMapObject);
    shadowMapShaderSkinned = ShaderLibrary::GetShader(ShaderLibrary::ShadowMapObjectSkinned);

    shadowDescription = Gfx::ImageDescription(shadowMapTexelSize.z, shadowMapTexelSize.w, Gfx::GfxFormat::D32_SFloat);

    shadowMap = GetGfxDriver()->CreateImage(
        shadowDescription,
        Gfx::ImageUsage::DepthStencilAttachment | Gfx::ImageUsage::Texture
    );
    shadowMapId = *shadowMap;

    pass.SetAttachment(0, shadowMapId);
}

void ShadowRenderer::SetSettings(ShadowRendererSettigns settings)
{ }

void ShadowRenderer::Execute(Gfx::CommandBuffer& cmd, DrawList& sceneDrawList)
{
    cmd.BeginLabel("Shadow Map", {0.11, 0.376, 0.729, 1.0});
    {
        if (updateMainLightShadow)
        {
            Gfx::ClearValue shadowMapClears[] = {{1.0f, 0}};
            cmd.BeginRenderPass(pass, shadowMapClears);
            auto program = shadowMapShader->GetShaderProgram();
            auto programSkinned = shadowMapShaderSkinned->GetShaderProgram();

            for (auto& draw : sceneDrawList)
            {
                auto programUsed = program;
                [[unlikely]]
                if (draw.skinned)
                {
                    programUsed = programSkinned;
                    if (draw.objectResource)
                        cmd.BindResource(1, draw.objectResource);
                }
                else
                {
                    auto ps = draw.GetPushConstant();
                    cmd.SetPushConstant(programUsed, (void*)&ps);
                }
                cmd.BindShaderProgram(programUsed, programUsed->GetDefaultShaderConfig());

                cmd.BindVertexBuffer(draw.vertexBufferBinding, 0);
                cmd.BindIndexBuffer(draw.indexBuffer, 0, draw.indexBufferType);
                cmd.DrawIndexed(draw.indexCount, 1, 0, 0, 0);
            }

            cmd.EndRenderPass();
        }
    }
}
} // namespace Rendering
  //
