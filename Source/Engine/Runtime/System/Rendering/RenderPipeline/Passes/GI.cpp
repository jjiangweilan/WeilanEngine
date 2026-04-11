#include "GI.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Runtime/Object/Component/Camera.hpp"
#include "Engine/Runtime/System/Rendering/GPUDriven/GPUDrivenManager.hpp"
#include "Engine/Runtime/System/Rendering/RenderingData.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"
#include <cstring>

namespace Rendering::Passes
{

static float Halton(int index, int base)
{
    float result = 0.0f;
    float f = 1.0f / base;
    int i = index;
    while (i > 0)
    {
        result += f * (i % base);
        i = i / base;
        f = f / base;
    }
    return result;
}

GI::GI()
{
    giShader = ShaderLibrary::GetShader(Shaders::GI_GI);
    mat.SetShader(giShader);

    // Generate Halton sequence (bases 2 & 3) and upload to GPU
    constexpr int haltonLength = 32;
    glm::vec2 haltonData[haltonLength];
    for (int i = 0; i < haltonLength; ++i)
        haltonData[i] = glm::vec2(Halton(i + 1, 2), Halton(i + 1, 3));

    haltonBuffer = GetGfxDriver()->CreateBuffer(
        sizeof(haltonData), Gfx::BufferUsage::Storage, true, false, "GI_HaltonSequence"
    );
    memcpy(haltonBuffer->GetCPUVisibleAddress(), haltonData, sizeof(haltonData));
}

void GI::EnsureHistoryBuffers(int width, int height)
{
    int quarterWidth = 4;
    int quarterHeight = 4;
    GetQuarterSize(width, height, quarterWidth, quarterHeight);

    if (historySize.x == quarterWidth && historySize.y == quarterHeight)
        return;

    historySize = {quarterWidth, quarterHeight};
    historyValid = false;

    Gfx::ImageDescription shDesc(quarterWidth, quarterHeight, Gfx::GfxFormat::R16G16B16A16_SFloat);
    auto usage = Gfx::ImageUsage::Texture | Gfx::ImageUsage::Storage | Gfx::ImageUsage::TransferDst | Gfx::ImageUsage::ColorAttachment;

    historySH0 = GetGfxDriver()->CreateImage(shDesc, usage);
    historySH0->SetName("GI_HistorySH0");
    historySH1 = GetGfxDriver()->CreateImage(shDesc, usage);
    historySH1->SetName("GI_HistorySH1");
    historySH2 = GetGfxDriver()->CreateImage(shDesc, usage);
    historySH2->SetName("GI_HistorySH2");

    GetGfxDriver()->InitGfxImage(*historySH0, float4(0, 0, 0, 0));
    GetGfxDriver()->InitGfxImage(*historySH1, float4(0, 0, 0, 0));
    GetGfxDriver()->InitGfxImage(*historySH2, float4(0, 0, 0, 0));
}

void GI::Execute(
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
    if (!setting->gi.enabled || tlas == -1 || rtContext == nullptr)
        return;

    cmd->BeginLabel("GI", {0.2, 0.7, 0.4, 1.0});

    int width = renderingData.screenSize.x;
    int height = renderingData.screenSize.y;
    int quarterWidth = 4;
    int quarterHeight = 4;
    GetQuarterSize(width, height, quarterWidth, quarterHeight);

    EnsureHistoryBuffers(width, height);

    glm::float4 rtSize(width, height, 1.0f / width, 1.0f / height);

    // Allocate quarter-resolution SH output attachments
    Gfx::RenderImageDescriptor shDesc(quarterWidth, quarterHeight, Gfx::GfxFormat::R16G16B16A16_SFloat);
    shDesc.SetRandomWrite(true);

    cmd->AllocateAttachment(giSH0, shDesc);
    cmd->AllocateAttachment(giSH1, shDesc);
    cmd->AllocateAttachment(giSH2, shDesc);

    // Bind full-res G-buffer inputs
    mat.SetTexture("hierarchyDepth", GetGfxDriver()->GetImageFromRenderGraph(hizTex));
    mat.SetTexture("albedoTex", GetGfxDriver()->GetImageFromRenderGraph(albedoTex));
    mat.SetTexture("normalTex", GetGfxDriver()->GetImageFromRenderGraph(normalTex));
    mat.SetTexture("noiseTex", renderingData.blueNoise.GetNoiseTexture());
    mat.SetBuffer("haltonSeq", haltonBuffer.get());
    mat.SetTexture("motionVectorTex", GetGfxDriver()->GetImageFromRenderGraph(motionVectorTex));

    // Bind SH history (quarter-res, from previous frame)
    mat.SetTexture("historySH0Tex", historySH0.get());
    mat.SetTexture("historySH1Tex", historySH1.get());
    mat.SetTexture("historySH2Tex", historySH2.get());

    // Bind SH outputs (quarter-res)
    mat.SetTexture("outSH0Tex", GetGfxDriver()->GetImageFromRenderGraph(giSH0));
    mat.SetTexture("outSH1Tex", GetGfxDriver()->GetImageFromRenderGraph(giSH1));
    mat.SetTexture("outSH2Tex", GetGfxDriver()->GetImageFromRenderGraph(giSH2));

    mat.SetVector("rtSize", rtSize);
    mat.SetFloat("secondary_bounce", setting->gi.secondary_bounce ? 1.0f : 0.0f);

    debugGI = setting->gi.debug_giOutput;

    auto* giProgram = mat.GetShaderProgram();
    mat.GetShaderResource()->SetAccelerationStructure("sceneBVH", 0, rtContext, tlas);

    cmd->BindResource(0, renderingData.globalResource);
    cmd->BindResource(mat.GetSet(Gfx::DescriptorSetSemantics::Material), mat.GetShaderResource());
    cmd->BindShaderProgram(giProgram, giProgram->GetDefaultShaderConfig());
    cmd->Dispatch((quarterWidth + 7) / 8, (quarterHeight + 7) / 8, 1);

    // Copy SH outputs to history for next frame
    cmd->Blit(Gfx::ImageIdentifier(*GetGfxDriver()->GetImageFromRenderGraph(giSH0)), Gfx::ImageIdentifier(*historySH0));
    cmd->Blit(Gfx::ImageIdentifier(*GetGfxDriver()->GetImageFromRenderGraph(giSH1)), Gfx::ImageIdentifier(*historySH1));
    cmd->Blit(Gfx::ImageIdentifier(*GetGfxDriver()->GetImageFromRenderGraph(giSH2)), Gfx::ImageIdentifier(*historySH2));

    historyValid = true;

    cmd->EndLabel(); // GI
}

bool GI::DebugBlit(Gfx::ImageIdentifier& dst)
{
    if (debugGI)
    {
        dst = giSH0;
        return true;
    }
    return false;
}

} // namespace Rendering::Passes
