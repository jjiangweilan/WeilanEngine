#include "ReflectionProbeUpdate.hpp"
#include "Core/Component/ReflectionProbe.hpp"
#include "Core/Scene/Scene.hpp"
#include "Rendering/RenderingUtils.hpp"
#include <span>

#define FFX_CPU 1
#include "Shaders/fidelityfx/ffx_common_types.h"
#include "Shaders/fidelityfx/ffx_core_cpu.h"
#include "Shaders/fidelityfx/spd/ffx_spd.h"

namespace Rendering::Passes
{

ReflectionProbeUpdate::ReflectionProbeUpdate(Gfx::Buffer* sceneBuffer, Gfx::Buffer* mainLightShadowBuffer)
{
    iblGenerator = ShaderLibrary::GetShader(Shaders::ReflectionProbeIBLGenerator);

    Gfx::ImageDescription cubemapDesc(
        256,
        256,
        1,
        Gfx::GfxFormat::B10G11R11_UFloat_Pack32,
        Gfx::MultiSampling::Sample_Count_1,
        6,
        true
    );
    cubemap = GetGfxDriver()->CreateImage(cubemapDesc, Gfx::ImageUsage::Storage | Gfx::ImageUsage::Texture);

    for (int i = 0; i < 30; i++)
    {
        uint32_t mip = i / 5;
        uint32_t face = i % 5;
        Gfx::ImageView::CreateInfo createInfo{
            *cubemap,
            Gfx::ImageViewType::Image_2D,
            Gfx::ImageSubresourceRange{
                Gfx::ImageAspect::Color,
                mip,
                1,
                face,
                1
            }
        };

        cubemapImageViews.push_back(GetGfxDriver()->CreateImageView(createInfo));
    }
}

void ReflectionProbeUpdate::Execute(Gfx::CommandBuffer& cmd, RenderingData& renderingData, ReflectionProbe& probe)
{
    auto shaderResource = EnsureProbeShaderResource(probe);

    cmd.BeginLabel("Reflection Probe IBL Generation", {0.4f, 0.1f, 0.7f, 1.0f});
    cmd.BindResource((int)Gfx::DescriptorSetSemantics::Material, shaderResource);

    cmd.BindShaderProgram(iblGenerator->GetShaderProgram(), iblGenerator->GetShaderProgram()->GetDefaultShaderConfig());
    cmd.EndLabel();

    int dispatchX = (probe.GetTotalPixelCount() + 63) / 64.0f;
    cmd.Dispatch(dispatchX, 1, 1);
}

Gfx::ShaderResource* ReflectionProbeUpdate::EnsureProbeShaderResource(ReflectionProbe& probe)
{
    auto it = probeShaderResources.find(probe.GetUUID());
    if (it != probeShaderResources.end())
    {
        return it->second.get();
    }

    auto shaderResource = GetGfxDriver()->CreateShaderResource();
    shaderResource->SetBuffer("input", *shaderInput);
    shaderResource->SetImage("srcCubemap", probe.GetCubemap());

    shaderInput->envMapSize = probe.GetCubemap()->GetDescription().width;
    shaderInput->envMapSizeSqr = shaderInput->envMapSize * shaderInput->envMapSize;
    shaderInput->roughness[0] = 0.0001;
    shaderInput->roughness[1] = 0.2;
    shaderInput->roughness[2] = 0.4;
    shaderInput->roughness[3] = 0.6;
    shaderInput->roughness[4] = 0.8;
    shaderInput->roughness[5] = 0.9999;

    GetGfxDriver()->UploadBuffer(**shaderInput, (uint8_t*)shaderInput.GetPtr(), shaderInput.GetSize());

    for (int i = 0; i < 30; ++i)
    {
        shaderResource->SetImage(Gfx::ShaderBindingHandle("dstFaces"), i, cubemapImageViews[i].get());
    }

    probeShaderResources[probe.GetUUID()] = std::move(shaderResource);
    return probeShaderResources[probe.GetUUID()].get();
}

void MipmapGeneration(uint32_t width, uint32_t height, uint32_t dispatchThreadGroupCountXY[])
{
    uint32_t workGroupOffset[2];
    uint32_t numWorkGroupsAndMips[2];
    uint32_t rectInfo[4] = {0, 0, width, height};
    ffxSpdSetup(dispatchThreadGroupCountXY, workGroupOffset, numWorkGroupsAndMips, rectInfo);


}
} // namespace Rendering::Passes
