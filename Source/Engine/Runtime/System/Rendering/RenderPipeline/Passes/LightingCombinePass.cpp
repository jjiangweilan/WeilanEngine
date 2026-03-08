#include "LightingCombinePass.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Runtime/Object/Component/Camera.hpp"
#include "Engine/Runtime/System/Rendering/RenderingData.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"

namespace Rendering::Passes
{
LightingCombinePass::LightingCombinePass()
{
    computeShader = ShaderLibrary::GetShader(Shaders::LightingCombine);
    mat.SetShader(computeShader);
}

void LightingCombinePass::Execute(
    Gfx::CommandBuffer* cmd,
    const Gfx::ImageIdentifier& ssil,
    const Gfx::ImageIdentifier& albedoTex,
    const Gfx::ImageIdentifier& colorTex,
    RenderingData& renderingData
)
{
    cmd->BeginLabel("LightingCombine", {0.8f, 0.4f, 0.2f, 1.0f});

    int width = renderingData.screenSize.x;
    int height = renderingData.screenSize.y;

    auto ssilImg = GetGfxDriver()->GetImageFromRenderGraph(ssil);
    mat.SetTexture("ssil", ssilImg);
    mat.SetTexture("albedoTex", GetGfxDriver()->GetImageFromRenderGraph(albedoTex));
    mat.SetTexture("colorTex", GetGfxDriver()->GetImageFromRenderGraph(colorTex));

    mat.SetVector("texelSize", glm::float4(1.0f / width, 1.0f / height, 0.0f, 0.0f));
    mat.SetVector("ssilTexelSize", glm::float4(1.0f / ssilImg->GetDescription().width, 1.0f / ssilImg->GetDescription().height, 0.0f, 0.0f));

    auto shaderProgram = mat.GetShaderProgram();
    cmd->BindResource(0, mat.GetShaderResource());
    cmd->BindShaderProgram(shaderProgram, shaderProgram->GetDefaultShaderConfig());
    cmd->Dispatch((width + 7) / 8, (height + 7) / 8, 1);

    cmd->EndLabel();
}

} // namespace Rendering::Passes
