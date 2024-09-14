#include "Core/Texture.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Rendering/Shader.hpp"

namespace Libs::Image
{
void GenerateIrradianceCubemap(float* source, int width, int height, int channels, float*& output)
{
    Gfx::ImageDescription imgDesc{};
    imgDesc.width = width;
    imgDesc.height = height;
    imgDesc.isCubemap = true;
    imgDesc.mipLevels = 1;
    imgDesc.format = Gfx::ImageFormat::R32G32B32A32_SFloat;
    imgDesc.depth = 1;
    imgDesc.multiSampling = Gfx::MultiSampling::Sample_Count_1;

    std::unique_ptr<Gfx::Image> sourceCuebmap = GetGfxDriver()->CreateImage(
        imgDesc,
        Gfx::ImageUsage::TransferDst | Gfx::ImageUsage::Texture | Gfx::ImageUsage::Storage
    );

    imgDesc.width = 512;
    imgDesc.height = 512;
    std::unique_ptr<Gfx::Image> dstCuebmap =
        GetGfxDriver()->CreateImage(imgDesc, Gfx::ImageUsage::Texture | Gfx::ImageUsage::Storage);

    auto cmd = GetGfxDriver()->CreateCommandBuffer();
    cmd->CopyDataToImage((uint8_t*)source, *dstCuebmap, width, height, 1, 1);
    cmd->SetTexture("_Src", *sourceCuebmap);
    cmd->SetTexture("_Dst", *dstCuebmap);

    ComputeShader compute;

    cmd->BindShaderProgram(compute.GetDefaultShaderProgram(), compute.GetDefaultShaderConfig());
    cmd->Dispatch(16, 16, 1);

    auto readbackBuf = GetGfxDriver()->CreateBuffer(imgDesc.GetByteSize(), Gfx::BufferUsage::Transfer_Dst, true);
    // copy processed image to cpu
    Gfx::BufferImageCopyRegion regions[] = {{
        .srcOffset = 0,
            .layers = {Gfx::ImageAspect::Color, 0, 0, 6},
            .offset = {0, 0, 0},
            .extend = {512, 512, 1}
    }};
    cmd->CopyImageToBuffer(dstCuebmap, readbackBuf, regions);

    GetGfxDriver()->ExecuteCommandBufferImmediately(*cmd);
}
} // namespace Libs::Image
