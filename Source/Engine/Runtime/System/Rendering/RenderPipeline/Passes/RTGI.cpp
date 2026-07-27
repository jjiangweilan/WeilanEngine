#include "RTGI.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Runtime/Object/Component/Camera.hpp"
#include "Engine/Runtime/System/Rendering/GPUDriven/GPUDrivenManager.hpp"
#include "Engine/Runtime/System/Rendering/RenderingData.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"

namespace Rendering::Passes
{

RTGI::RTGI()
{
    rtgiShader = ShaderLibrary::GetShader(Shaders::PostProcess_RTGI);

    mat.SetShader(rtgiShader);
}


void RTGI::Execute(
    Gfx::CommandBuffer* cmd,
    const Gfx::ImageIdentifier& hizTex,
    const Gfx::ImageIdentifier& albedoTex,
    const Gfx::ImageIdentifier& normalTex,
    const Gfx::ImageIdentifier& motionVectorTex,
    RenderPipelineSetting* setting,
    RenderingData& renderingData,
    Gfx::RayTracingSceneHandle tlas,
    Gfx::RayTracingContext* rtContext
)
{
    if (!setting->rtgi.enabled || tlas == -1 || rtContext == nullptr)
        return;

    cmd->BeginLabel("RTGI", {0.8, 0.4, 0.1, 1.0});

    int width = renderingData.screenSize.x;
    int height = renderingData.screenSize.y;

    glm::float4 rtSize(width, height, 1.0f / width, 1.0f / height);

    Gfx::RenderImageDescriptor colorDesc(width, height, Gfx::GfxFormat::R16G16B16A16_SFloat);
    colorDesc.SetRandomWrite(true);

    // -------------------------------------------------------------------------
    // Pass 1: Ray tracing — one sample per pixel into rtgiRaw
    // -------------------------------------------------------------------------
    cmd->BeginLabel("RTGI_RT", {0.8, 0.4, 0.1, 1.0});

    cmd->AllocateAttachment(rtgiRaw, colorDesc);

    mat.SetTexture("depthTex", GetGfxDriver()->GetImageFromRenderGraph(hizTex));
    mat.SetTexture("albedoTex", GetGfxDriver()->GetImageFromRenderGraph(albedoTex));
    mat.SetTexture("normalTex", GetGfxDriver()->GetImageFromRenderGraph(normalTex));
    mat.SetTexture("noiseTex", renderingData.blueNoise.GetNoiseTexture());
    mat.SetTexture("environmentMap", renderingData.specularCubemap);
    mat.SetTexture("outRtgiTex", GetGfxDriver()->GetImageFromRenderGraph(rtgiRaw));
    mat.SetVector("rtSize", rtSize);
    mat.SetFloat("ray_multiplier", (float)setting->rtgi.ray_multiplier);
    mat.SetFloat("secondary_bounce", setting->rtgi.secondary_bounce ? 1.0f : 0.0f);

    debugRTGI = setting->rtgi.debug_rtgiOutput;

    auto* rtProgram = mat.GetShaderProgram();
    mat.GetShaderResource()->SetAccelerationStructure("sceneBVH", 0, rtContext, tlas);

    cmd->BindResource(0, renderingData.globalResource);
    cmd->BindResource(mat.GetSet(Gfx::DescriptorSetSemantics::Material), mat.GetShaderResource());
    cmd->BindShaderProgram(rtProgram, rtProgram->GetDefaultPipelineConfig());
    cmd->Dispatch((width + 7) / 8, (height + 7) / 8, 1);

    cmd->EndLabel();

    rtgi = rtgiRaw;
    cmd->EndLabel(); // RTGI
}

bool RTGI::DebugBlit(Gfx::ImageIdentifier& dst)
{
    if (debugRTGI)
    {
        dst = rtgi;
        return true;
    }
    return false;
}

} // namespace Rendering::Passes
