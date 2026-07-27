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
    const Gfx::ImageIdentifier* rtgi,
    const Gfx::ImageIdentifier* giIrradianceTex,
    const Gfx::ImageIdentifier& ssaoTex,
    const Gfx::ImageIdentifier& albedoTex,
    const Gfx::ImageIdentifier& colorTex,
    RenderingData& renderingData
)
{
    cmd->BeginLabel("LightingCombine", {0.8f, 0.4f, 0.2f, 1.0f});

    int width = renderingData.screenSize.x;
    int height = renderingData.screenSize.y;

    auto albedoImg = GetGfxDriver()->GetImageFromRenderGraph(albedoTex);

    int hasRTGI = 0;
    if (rtgi)
    {
        auto rtgiImg = GetGfxDriver()->GetImageFromRenderGraph(*rtgi);
        mat.SetTexture("rtgiTex", rtgiImg);
        hasRTGI = 1;
    }
    else
    {
        mat.SetTexture("rtgiTex", albedoImg); // dummy
    }

    int hasGI = 0;
    if (giIrradianceTex)
    {
        mat.SetTexture("giIrradianceTex", GetGfxDriver()->GetImageFromRenderGraph(*giIrradianceTex));
        hasGI = 1;
    }
    else
    {
        mat.SetTexture("giIrradianceTex", albedoImg); // dummy
    }

    mat.SetTexture("ssaoTex", GetGfxDriver()->GetImageFromRenderGraph(ssaoTex));
    mat.SetTexture("albedoTex", albedoImg);
    mat.SetTexture("colorTex", GetGfxDriver()->GetImageFromRenderGraph(colorTex));

    mat.SetVector("texelSize", glm::float4(1.0f / width, 1.0f / height, (float)width, (float)height));
    mat.SetVector("flags", glm::float4(hasRTGI, hasGI, 0.0f, 0.0f));
    mat.SetFloat(
        "giIntensityScale",
        renderingData.renderPipelineSettings->gi.intensityScale * 3.1415926f
    );
    auto shaderProgram = mat.GetShaderProgram();
    cmd->BindResource(mat.GetSet(Gfx::DescriptorSetSemantics::Material), mat.GetShaderResource());
    cmd->BindShaderProgram(shaderProgram, shaderProgram->GetDefaultPipelineConfig());
    cmd->Dispatch((width + 7) / 8, (height + 7) / 8, 1);

    cmd->EndLabel();
}

} // namespace Rendering::Passes
