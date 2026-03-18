#include "ReflectionProbeUpdate.hpp"
#include "Engine/Runtime/Object/Component/ReflectionProbe.hpp"
#include "Engine/Runtime/System/SceneManager/Scene.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/PerScene.hpp"
#include "Engine/Runtime/System/Rendering/RenderingUtils.hpp"
#include <span>

#define FFX_CPU 1
#include "Engine/Shaders/fidelityfx/ffx_common_types.h"
#include "Engine/Shaders/fidelityfx/ffx_core_cpu.h"
#include "Engine/Shaders/fidelityfx/spd/ffx_spd.h"

namespace Rendering::Passes
{

void ReflectionProbeUpdate::Execute(Gfx::CommandBuffer& cmd, RenderingData& renderingData)
{
    auto shaderResource = EnsureAndGetShaderResource(cubemapImageViews);

    cmd.BeginLabel("Reflection Probe IBL Generation", {0.4f, 0.1f, 0.7f, 1.0f});

    DrawSkyboxOnProbe(cmd, *cubemapBase, cubemapBaseMat, *renderingData.globalResource);

    auto srcProbeBase = cubemapBase.get();
    MipmapGeneration(cmd, srcProbeBase->GetDescription().width, srcProbeBase->GetDescription().height, *srcProbeBase);

    cmd.BindResource((int)Gfx::DescriptorSetSemantics::Global, shaderResource);

    cmd.BindShaderProgram(iblGenerator->GetShaderProgram(), iblGenerator->GetShaderProgram()->GetDefaultShaderConfig());

    int dispatchX = (totalPixelCount + 63) / 64.0f;
    cmd.Dispatch(dispatchX, 1, 1);

    cmd.EndLabel();
}

void ReflectionProbeUpdate::MipmapGeneration(Gfx::CommandBuffer& cmd, uint32_t width, uint32_t height, Gfx::Image& src)
{
    uint32_t dispatchThreadGroupCountXY[2];
    uint32_t workGroupOffset[2];
    uint32_t numWorkGroupsAndMips[2];
    uint32_t rectInfo[4] = {0, 0, width, height};
    ffxSpdSetup(dispatchThreadGroupCountXY, workGroupOffset, numWorkGroupsAndMips, rectInfo);

    auto spdBufferVal = *spdBuffer.GetPtr();
    spdBufferVal.mips = numWorkGroupsAndMips[1];
    spdBufferVal.numWorkGroups = numWorkGroupsAndMips[0];
    spdBufferVal.workGroupOffset = uint2(workGroupOffset[0], workGroupOffset[0]);
    spdBufferVal.invInputSize = float2(1.0f / width, 1.0f / height);
    if (memcmp(&spdBufferVal, spdBuffer.GetPtr(), sizeof(ffx_spd_resources)) != 0)
    {
        GetGfxDriver()->UploadBuffer(*spdBuffer, (uint8_t*)&spdBufferVal, sizeof(ffx_spd_resources));
    }

    spdInput->SetImage("r_input_downsample_src", &src);
    spdInput->SetBuffer("rw_internal_global_atomic", spdGlobalAtomic.get());
    spdInput->SetBuffer("spdInput", &*spdBuffer);

    auto layerCount = src.GetDescription().GetLayer();
    for (int mip = 0; mip < src.GetDescription().mipLevels; mip++)
    {
        Gfx::ImageViewOption imageViewOpt{mip, 1, 0, (int)layerCount, Gfx::ImageAspect::Color};
        auto& imageView = src.GetImageView(imageViewOpt);

        if (mip == 6)
            spdInput->SetImage(Gfx::ShaderBindingHandle("rw_input_downsample_src_mid_mip"), &imageView);
        else
            spdInput->SetImage(Gfx::ShaderBindingHandle("rw_input_downsample_src_mips"), mip, &imageView);
    }

    const int cubeFaces = 6;
    auto program = spdShader->GetShaderProgram();
    cmd.BindResource(0, spdInput.get());
    cmd.BindShaderProgram(program, program->GetDefaultShaderConfig());
    cmd.Dispatch(dispatchThreadGroupCountXY[0], dispatchThreadGroupCountXY[1], cubeFaces);
}

void ReflectionProbeUpdate::DrawSkyboxOnProbe(Gfx::CommandBuffer& cmd, Gfx::Image& probe, Material& baseMat, Gfx::ShaderResource& globalSet)
{
    cmd.BindResource(0, &globalSet);
    cmd.BindResource(1, baseMat.GetShaderResource());

    cmd.BindShaderProgram(baseMat.GetShaderProgram(), baseMat.GetShaderConfig());
    cmd.Dispatch(probe.GetDescription().width / 8, probe.GetDescription().height / 8, 6);
}

void ReflectionProbeUpdate::OnInit(RenderingData* renderingData)
{
    Gfx::Buffer* sceneBuffer = renderingData->perScene->scene.get();
    Gfx::Buffer* mainLightShadowBuffer = renderingData->perScene->mainLightShadow.get();

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
    cubemap->SetName("Reflection Probe IBL Cubemap");

    for (int i = 0; i < 36; i++)
    {
        uint32_t mip = i % 6;
        uint32_t face = i / 6;
        Gfx::ImageView::CreateInfo createInfo{
            cubemap.get(),
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

    totalPixelCount = 0;
    for (int mip = 0; mip < cubemapDesc.mipLevels; mip++)
    {
        int mipWidth = cubemapDesc.width * glm::pow(0.5, mip);
        int mipHeight = cubemapDesc.height * glm::pow(0.5, mip);

        int facePixelCount = mipWidth * mipHeight;
        int mipPixelCount = facePixelCount * 6;

        totalPixelCount += mipPixelCount;
    }

    roughness[0] = 0.01f;
    roughness[1] = 0.2f;
    roughness[2] = 0.4f;
    roughness[3] = 0.6f;
    roughness[4] = 0.8f;
    roughness[5] = 0.999f;

    cubemapDesc.mipLevels = (int)glm::log2((float)cubemapDesc.width) + 1;
    cubemapBase = GetGfxDriver()->CreateImage(cubemapDesc, Gfx::ImageUsage::Texture | Gfx::ImageUsage::ColorAttachment | Gfx::ImageUsage::Storage);
    cubemapBase->SetName("Reflection Probe Base Cubemap");
    cubemapBaseMat.SetShader(Shaders::ReflectionProbeSkybox);
    float width = cubemapBase->GetDescription().width;
    cubemapBaseMat.SetVector("resolution", float4(width, width, 1.0f / width, 1.0f / width));

    auto& imageView = cubemapBase->GetImageView(Gfx::ImageViewOption(0, 1, 0, Gfx::Remaining_Array_Layers, Gfx::ImageAspect::Color));

    cubemapBaseMat.GetShaderResource()->SetImage("outputCubemap", &imageView);

    // Setup spd //
    spdShader = ShaderLibrary::GetShader(Shaders::FidelityFX_SPD);
    spdInput = GetGfxDriver()->CreateShaderResource();

    spdGlobalAtomic = GetGfxDriver()->CreateBuffer(sizeof(uint32_t) * 6, Gfx::BufferUsage::Storage, false, true, "SPD Global Atomic");

    Gfx::ImageDescription rw_input_downsample_src_mid_mipDesc(cubemapDesc.width, cubemapDesc.height, cubemapDesc.format);
    rw_input_downsample_src_mid_mipDesc.layers = (uint32_t)glm::log2((float)cubemapDesc.width) + 1;
    rw_input_downsample_src_mid_mip = GetGfxDriver()->CreateImage(rw_input_downsample_src_mid_mipDesc, Gfx::ImageUsage::Storage);
}

Gfx::ShaderResource* ReflectionProbeUpdate::EnsureAndGetShaderResource(std::vector<std::unique_ptr<Gfx::ImageView>>& cubemapImageViews)
{
    if (probeUpdateShaderResource != nullptr)
    {
        return probeUpdateShaderResource.get();
    }

    probeUpdateShaderResource = GetGfxDriver()->CreateShaderResource();
    probeUpdateShaderResource->SetBuffer("input", &*shaderInput);
    probeUpdateShaderResource->SetImage("srcCubemap", cubemapBase.get());

    shaderInput->envMapSize = cubemap->GetDescription().width;
    shaderInput->envMapSizeSqr = shaderInput->envMapSize * shaderInput->envMapSize;
    shaderInput->totalPixelCount = totalPixelCount;
    shaderInput->roughness[0] = 0.0001;
    shaderInput->roughness[1] = 0.2;
    shaderInput->roughness[2] = 0.4;
    shaderInput->roughness[3] = 0.6;
    shaderInput->roughness[4] = 0.8;
    shaderInput->roughness[5] = 0.9999;

    GetGfxDriver()->UploadBuffer(*shaderInput, (uint8_t*)shaderInput.GetPtr(), shaderInput.GetSize());

    for (int i = 0; i < 36; ++i)
    {
        probeUpdateShaderResource->SetImage(Gfx::ShaderBindingHandle("dstFaces"), i, cubemapImageViews[i].get());
    }

    return probeUpdateShaderResource.get();
}
} // namespace Rendering::Passes
