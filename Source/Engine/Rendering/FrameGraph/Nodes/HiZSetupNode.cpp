#pragma once
#include "../NodeBlueprint.hpp"
#include "Asset/Shader.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include <glm/glm.hpp>

namespace FrameGraph
{
class HiZSetupNode : public Node
{
    DECLARE_FRAME_GRAPH_NODE(HiZSetupNode)
    {
        input.depth = AddInputProperty("source depth", PropertyType::Attachment);
        AddOutputProperty("HiZ *", PropertyType::GraphFlow);

        hizDescs.resize(MAX_MIP);
        hizIds.resize(MAX_MIP);

        spdAtomicCounter = GetGfxDriver()->CreateBuffer(Gfx::Buffer::CreateInfo{
            .usages = Gfx::BufferUsage::Storage | Gfx::BufferUsage::Transfer_Dst,
            .size = sizeof(uint32_t) * 6, // 6 is defined ffx_spd_callbacks_glsl
            .visibleInCPU = false,
            .debugName = "rw_internal_global_atomic",
            .gpuWrite = true});

        spdUbo = GetGfxDriver()->CreateBuffer(Gfx::Buffer::CreateInfo{
            .usages = Gfx::BufferUsage::Uniform | Gfx::BufferUsage::Transfer_Dst,
            .size = sizeof(cbFSR1_t),
            .visibleInCPU = false,
            .debugName = "spd cbFSR1_t",
            .gpuWrite = true});

        hiZSetupCompute =
            static_cast<ComputeShader*>(AssetDatabase::Singleton()->LoadAsset("_engine_internal/Shaders/HiZSetup.comp")
            );
    }

public:
    void Compile() override {}

    void Execute(Gfx::CommandBuffer& cmd, RenderingData& renderingData) override
    {
        if (hiZSetupCompute == nullptr)
            return;

        auto srcDepth = input.depth->GetValue<AttachmentProperty>();
        int mipCount = glm::min(
            glm::floor(glm::log2(float(glm::max(srcDepth.desc.GetWidth(), srcDepth.desc.GetHeight()))) + 1),
            12.0f
        );

        glm::uvec2 dispatchThreadGroupCountXY;
        glm::uvec2 workGroupOffset;
        glm::uvec2 numWorkGroupsAndMips;
        glm::uvec4 rect = {0, 0, srcDepth.desc.GetWidth(), srcDepth.desc.GetHeight()};
        ffxSpdSetup(rect, mipCount, dispatchThreadGroupCountXY, workGroupOffset, numWorkGroupsAndMips);
        cbFSR1.mips = numWorkGroupsAndMips.y;
        cbFSR1.numWorkGroups = numWorkGroupsAndMips.x;
        cbFSR1.workGroupOffset = workGroupOffset;
        cbFSR1.invInputSize = {1.0f / srcDepth.desc.GetWidth(), 1.0f / srcDepth.desc.GetHeight()};
        GetGfxDriver()->UploadBuffer(*spdUbo, (uint8_t*)&cbFSR1, sizeof(cbFSR1));

        cmd.SetTexture(inputSrcBinding, srcDepth.id);
        cmd.SetBuffer("rw_internal_global_atomic_t", *spdAtomicCounter);
        cmd.SetBuffer("cbFSR1_t", *spdUbo);
        for (int i = 1; i < mipCount; ++i)
        {
            float mipScale = (1 << i);
            hizDescs[i].SetFormat(Gfx::ImageFormat::R32_Float);
            hizDescs[i].SetWidth(glm::ceil(srcDepth.desc.GetWidth() / mipScale));
            hizDescs[i].SetHeight(glm::ceil(srcDepth.desc.GetHeight() / mipScale));
            hizDescs[i].SetRandomWrite(true);
            cmd.AllocateAttachment(hizIds[i], hizDescs[i]);
            if (i == 6)
            {
                cmd.SetTexture(srcMidMipBinding, hizIds[i]);
            }
            else
            {
                cmd.SetTexture(srcMipBinding, i, hizIds[i]);
            }
        }
        cmd.BindShaderProgram(hiZSetupCompute->GetDefaultShaderProgram(), hiZSetupCompute->GetDefaultShaderConfig());
        cmd.Dispatch(dispatchThreadGroupCountXY.x, dispatchThreadGroupCountXY.y, 1);

        cmd.SetTexture(hizBuffers, 0, srcDepth.id);
        for (int i = 1; i < mipCount; ++i)
        {
            cmd.SetTexture(hizBuffers, i, hizIds[i]);
        }
    }

private:
    struct
    {
        PropertyHandle depth;
    } input;

    struct
    {
    } output;

    // defined in ffx_spd_callbacks_glsl.h
    struct cbFSR1_t
    {
        uint32_t mips;
        uint32_t numWorkGroups;
        glm::uvec2 workGroupOffset;
        glm::vec2 invInputSize;
        glm::vec2 padding;
    } cbFSR1;

    std::vector<Gfx::RG::ImageDescription> hizDescs;
    std::vector<Gfx::RG::ImageIdentifier> hizIds;
    std::unique_ptr<Gfx::Buffer> spdAtomicCounter;
    std::unique_ptr<Gfx::Buffer> spdUbo;
    ComputeShader* hiZSetupCompute;

    Gfx::ShaderBindingHandle hizBuffers = Gfx::ShaderBindingHandle("hiZBuffers");
    Gfx::ShaderBindingHandle srcMipBinding = Gfx::ShaderBindingHandle("rw_input_downsample_src_mips");
    Gfx::ShaderBindingHandle srcMidMipBinding = Gfx::ShaderBindingHandle("rw_input_downsample_src_mid_mip");
    Gfx::ShaderBindingHandle inputSrcBinding = Gfx::ShaderBindingHandle("r_input_downsample_src");

    // maximum mip levels is defined in ffx_spd_resources.h
    const int MAX_MIP = 12;

    // port from ffx_spd.h
    void ffxSpdSetup(
        glm::uvec4 rectInfo,
        int32_t mips,
        glm::uvec2& dispatchThreadGroupCountXY,
        glm::uvec2& workGroupOffset,
        glm::uvec2& numWorkGroupsAndMips
    )
    {
        // determines the offset of the first tile to downsample based on
        // left (rectInfo[0]) and top (rectInfo[1]) of the subregion.
        workGroupOffset[0] = rectInfo[0] / 64;
        workGroupOffset[1] = rectInfo[1] / 64;

        uint32_t endIndexX = (rectInfo[0] + rectInfo[2] - 1) / 64; // rectInfo[0] = left, rectInfo[2] = width
        uint32_t endIndexY = (rectInfo[1] + rectInfo[3] - 1) / 64; // rectInfo[1] = top, rectInfo[3] = height

        // we only need to dispatch as many thread groups as tiles we need to downsample
        // number of tiles per slice depends on the subregion to downsample
        dispatchThreadGroupCountXY[0] = endIndexX + 1 - workGroupOffset[0];
        dispatchThreadGroupCountXY[1] = endIndexY + 1 - workGroupOffset[1];

        // number of thread groups per slice
        numWorkGroupsAndMips[0] = (dispatchThreadGroupCountXY[0]) * (dispatchThreadGroupCountXY[1]);

        if (mips >= 0)
        {
            numWorkGroupsAndMips[1] = uint32_t(mips);
        }
        else
        {
            // calculate based on rect width and height
            uint32_t resolution = glm::max(rectInfo[2], rectInfo[3]);
            numWorkGroupsAndMips[1] = uint32_t((glm::min(floor(log2(float(resolution))), float(MAX_MIP))));
        }
    }
}; // namespace FrameGraph

DEFINE_FRAME_GRAPH_NODE(HiZSetupNode, "C4147D77-145B-405F-9B02-CADF10CB4F86")
} // namespace FrameGraph
