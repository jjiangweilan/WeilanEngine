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
    const Gfx::ImageIdentifier* ssil,
    const Gfx::ImageIdentifier* rtgi,
    const Gfx::ImageIdentifier* rtgiSH0,
    const Gfx::ImageIdentifier* rtgiSH1,
    const Gfx::ImageIdentifier* rtgiSH2,
    const Gfx::ImageIdentifier& albedoTex,
    const Gfx::ImageIdentifier& normalTex,
    const Gfx::ImageIdentifier& colorTex,
    RenderingData& renderingData
)
{
    cmd->BeginLabel("LightingCombine", {0.8f, 0.4f, 0.2f, 1.0f});

    int width = renderingData.screenSize.x;
    int height = renderingData.screenSize.y;

    auto albedoImg = GetGfxDriver()->GetImageFromRenderGraph(albedoTex);

    int hasSSIL = 0;
    if (ssil)
    {
        auto ssilImg = GetGfxDriver()->GetImageFromRenderGraph(*ssil);
        mat.SetTexture("ssil", ssilImg);
        hasSSIL = 1;
    }
    else
    {
        mat.SetTexture("ssil", albedoImg); // dummy
    }

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

    int hasGISH = 0;
    if (rtgiSH0 && rtgiSH1 && rtgiSH2)
    {
        mat.SetTexture("rtgiSH0Tex", GetGfxDriver()->GetImageFromRenderGraph(*rtgiSH0));
        mat.SetTexture("rtgiSH1Tex", GetGfxDriver()->GetImageFromRenderGraph(*rtgiSH1));
        mat.SetTexture("rtgiSH2Tex", GetGfxDriver()->GetImageFromRenderGraph(*rtgiSH2));
        hasGISH = 1;
    }
    else
    {
        mat.SetTexture("rtgiSH0Tex", albedoImg); // dummy
        mat.SetTexture("rtgiSH1Tex", albedoImg);
        mat.SetTexture("rtgiSH2Tex", albedoImg);
    }

    mat.SetTexture("albedoTex", albedoImg);
    mat.SetTexture("normalTex", GetGfxDriver()->GetImageFromRenderGraph(normalTex));
    mat.SetTexture("colorTex", GetGfxDriver()->GetImageFromRenderGraph(colorTex));

    mat.SetVector("texelSize", glm::float4(1.0f / width, 1.0f / height, 0.0f, 0.0f));
    mat.SetVector("flags", glm::float4(hasSSIL, hasRTGI, hasGISH, 0.0f));
    auto shaderProgram = mat.GetShaderProgram();
    cmd->BindResource(0, mat.GetShaderResource());
    cmd->BindShaderProgram(shaderProgram, shaderProgram->GetDefaultShaderConfig());
    cmd->Dispatch((width + 7) / 8, (height + 7) / 8, 1);

    cmd->EndLabel();
}

} // namespace Rendering::Passes
