#include "DepthDownSampler.hpp"
#include "Engine/Driver/GfxDriver/CommandBuffer.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"

namespace Rendering::Passes
{

DepthDownSampler::DepthDownSampler()
{
    shader = ShaderLibrary::GetShader(Shaders::DepthDownSampler);
    resource.SetShader(shader);
    resource.SetName("DepthDownSampler Resource");
}
void DepthDownSampler::Execute(Gfx::CommandBuffer& cmd)
{
    cmd.BeginLabel("DepthDownSampler", {0.13, .532, 0.367, 1.0f});
    int dstWidth = dstDepthDesc.GetWidth();
    int dstHeight = dstDepthDesc.GetHeight();
    resource.SetTexture("src", GetGfxDriver()->GetImageFromRenderGraph(srcDepth));
    resource.SetTexture("dst", GetGfxDriver()->GetImageFromRenderGraph(dstDepth));
    resource.SetVector("dstTexelSize", glm::vec4(1.0f / dstWidth, 1.0f / dstHeight, dstWidth, dstHeight));
    cmd.BindShaderProgram(shader->GetShaderProgram(), shader->GetShaderProgram()->GetDefaultShaderConfig());
    cmd.BindResource(resource.GetSet(Gfx::DescriptorSetSemantics::Material), resource.GetShaderResource());
    cmd.Dispatch((dstWidth + 7) / 8, (dstHeight + 7) / 8, 1);
    cmd.EndLabel();
}
} // namespace Rendering::Passes
