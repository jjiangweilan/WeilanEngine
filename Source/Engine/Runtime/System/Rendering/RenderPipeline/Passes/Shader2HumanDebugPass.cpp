#include "Shader2HumanDebugPass.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Runtime/System/Rendering/RenderingData.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"

namespace Rendering::Passes
{

Shader2HumanDebugPass::Shader2HumanDebugPass()
{
    mergeShader = ShaderLibrary::GetShader(Shaders::S2HDebug_Compute);
    if (mergeShader)
    {
        mergeMat.SetShader(mergeShader);
    }
}

void Shader2HumanDebugPass::Execute(
    Gfx::CommandBuffer* cmd,
    const Gfx::ImageIdentifier& giS2HDebug,
    const Gfx::ImageIdentifier& hizTex,
    const Gfx::ImageIdentifier& mainColorTex,
    glm::int2 screenSize,
    const RenderingData& renderingData
)
{
    if (!mergeShader)
        return;

    cmd->BeginLabel("Shader2HumanDebugPass", {0.8f, 0.2f, 0.8f, 1.0f});

    mergeMat.SetTexture("debugTex", GetGfxDriver()->GetImageFromRenderGraph(giS2HDebug));
    mergeMat.SetTexture("depthTex", GetGfxDriver()->GetImageFromRenderGraph(hizTex));
    mergeMat.SetTexture("outColorTex", GetGfxDriver()->GetImageFromRenderGraph(mainColorTex));

    auto* mergeProgram = mergeMat.GetShaderProgram();
    cmd->BindResource(0, renderingData.globalResource);
    cmd->BindResource(mergeMat.GetSet(Gfx::DescriptorSetSemantics::Material), mergeMat.GetShaderResource());
    cmd->BindShaderProgram(mergeProgram, mergeProgram->GetDefaultPipelineConfig());
    
    int width = screenSize.x;
    int height = screenSize.y;
    cmd->Dispatch((width + 7) / 8, (height + 7) / 8, 1);

    cmd->EndLabel();
}

}