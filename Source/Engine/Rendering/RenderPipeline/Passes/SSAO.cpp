#include "SSAO.hpp"

namespace Rendering::Passes
{
SSAO::SSAO()
{
    ssaoShader = ShaderLibrary::GetShader(ShaderLibrary::PostProcess_SSAO);
    mat.SetShader(ssaoShader);
}

void SSAO::Execute(
    Gfx::CommandBuffer* cmd,
    const Gfx::RG::ImageIdentifier& texDepth,
    const Gfx::RG::ImageDescription& depthTexDesc,
    RenderPipelineSetting* setting
)
{
    mat.SetFloat("strength", setting->ssao.strength);
    mat.SetFloat("scaling", setting->ssao.scaling);
    mat.SetFloat("falloff", setting->ssao.falloff);
    mat.SetFloat("bias", setting->ssao.bias);

    mat.SetVector(
        "rtSize",
        glm::float4(
            depthTexDesc.GetWidth(),
            depthTexDesc.GetHeight(),
            1.0f / depthTexDesc.GetWidth(),
            1.0f / depthTexDesc.GetHeight()
        )
    );
    mat.SetTexture("depthTex", GetGfxDriver()->GetImageFromRenderGraph(texDepth));

    Gfx::ClearValue clears[] = {{1.0f, 1.0f, 1.0f, 1.0f}};
    Gfx::RG::ImageDescription desc(depthTexDesc.GetWidth(), depthTexDesc.GetHeight(), Gfx::GfxFormat::R32_SFloat);

    cmd->AllocateAttachment(ssao, desc);

    pass.SetAttachment(0, ssao);
    cmd->BeginLabel("SSAO", {0.3, 0.1, 0.5, 1.0});
    cmd->BeginRenderPass(pass, clears);
    if (setting->ssao.enabled)
    {
        auto shaderProgram = mat.GetShaderProgram();
        cmd->BindResource(mat.GetSet(Gfx::DescriptorSetSemantics::Material), mat.GetShaderResource());
        cmd->BindShaderProgram(shaderProgram, shaderProgram->GetDefaultShaderConfig());
        cmd->Draw(6, 1, 0, 0);
    }
    cmd->EndRenderPass();
    cmd->EndLabel(); // SSAO
}

} // namespace Rendering::Passes
