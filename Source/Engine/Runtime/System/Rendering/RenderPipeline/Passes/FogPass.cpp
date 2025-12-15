#include "FogPass.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"

namespace Rendering::Passes
{
FogPass::FogPass()
{
    shaderInput = GetGfxDriver()->CreateShaderResource();
    shaderInput->SetBuffer("shaderInput", &*fogInputBuffer);
    shader = ShaderLibrary::GetShader(Shaders::DepthBasedFog);
}

void FogPass::Execute(Gfx::CommandBuffer& cmd, Gfx::ImageIdentifier& outputColor, Gfx::ImageIdentifier& depthCopy, const FogPassParameters& parameters)
{
    if (parameters.enabled)
    {
        auto fogParams = *fogInputBuffer.GetPtr();
        fogParams.fogColor = parameters.fogColor;
        fogParams.fogDensity = parameters.fogDensity;

        fogInputBuffer.SetAndUpload(fogParams);

        shaderInput->SetImage("depthTexture"_shaderBinding, depthCopy);

        Gfx::RenderAttachment color[] = {{outputColor, Gfx::AttachmentLoadOperation::Load}};
        Gfx::ClearValue clears[] = {{0, 0, 0, 0}};
        cmd.BeginRenderPass(color, clears);

        auto shaderProgram = shader->GetShaderProgram();
        cmd.BindShaderProgram(shaderProgram, shaderProgram->GetDefaultShaderConfig());
        cmd.BindResource(1, shaderInput.get());
        // cmd.Draw(6, 1, 0, 0);

        cmd.EndRenderPass();
    }
}
} // namespace Rendering::Passes
