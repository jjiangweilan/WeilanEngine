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

    // Setup spd //
    ffxSpd.SetShader(Shaders::FidelityFX_SPD);

    spdGlobalAtomic = GetGfxDriver()->CreateBuffer(sizeof(uint32_t) * 6, Gfx::BufferUsage::Storage, false, true, "SPD Global Atomic");

    Gfx::ImageDescription rw_input_downsample_src_mid_mipDesc(cubemapDesc.width, cubemapDesc.height, cubemapDesc.format);
    rw_input_downsample_src_mid_mipDesc.layers = (uint32_t)glm::log2((float)cubemapDesc.width);
    rw_input_downsample_src_mid_mip = GetGfxDriver()->CreateImage(rw_input_downsample_src_mid_mipDesc, Gfx::ImageUsage::Storage);

}

void ReflectionProbeUpdate::Execute(Gfx::CommandBuffer& cmd, RenderingData& renderingData, ReflectionProbe& probe)
{
    auto shaderResource = EnsureProbeShaderResource(probe);

    cmd.BeginLabel("Reflection Probe IBL Generation", {0.4f, 0.1f, 0.7f, 1.0f});

    // MipmapGeneration(cmd, 1, 1);

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

void ReflectionProbeUpdate::MipmapGeneration(Gfx::CommandBuffer& cmd, uint32_t width, uint32_t height, Gfx::Image& src)
{
    uint32_t dispatchThreadGroupCountXY[2];
    uint32_t workGroupOffset[2];
    uint32_t numWorkGroupsAndMips[2];
    uint32_t rectInfo[4] = {0, 0, width, height};
    ffxSpdSetup(dispatchThreadGroupCountXY, workGroupOffset, numWorkGroupsAndMips, rectInfo);

    ffxSpd.SetFloat("mips", numWorkGroupsAndMips[1]);
    ffxSpd.SetFloat("numWorkGroups", numWorkGroupsAndMips[0]);
    ffxSpd.SetVector("workGroupOffset", float4(workGroupOffset[0], workGroupOffset[1], 0, 0));
    ffxSpd.SetVector("invInputSize", float4(1.0f / width, 1.0f / height, 0, 0));
    ffxSpd.SetTexture("r_input_downsample_src", &src);
    ffxSpd.SetTexture("rw_input_downsample_src_mid_mip", rw_input_downsample_src_mid_mip.get());
    ffxSpd.SetBuffer("rw_internal_global_atomic", spdGlobalAtomic.get());

    Gfx::ImageViewOption mipView{0, 1, 0, 6, Gfx::ImageAspect::Color};
    // src.GetImageView();
    // ffxSpd.GetShaderResource()->SetImage(Gfx::ShaderBindingHandle("rw_input_downsample_src_mips"), 0, );

    const int cubeFaces = 6;
    cmd.BindResource(1, ffxSpd.GetShaderResource());
    cmd.Dispatch(dispatchThreadGroupCountXY[0], dispatchThreadGroupCountXY[1], cubeFaces);
}
} // namespace Rendering::Passes
