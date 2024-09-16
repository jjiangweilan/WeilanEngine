#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Texture.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Rendering/Shader.hpp"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "ThirdParty/stb/stb_image_write.h"
#include "AssetDatabase/AssetDatabase.hpp"
#include "AssetDatabase/Exporters/KtxExporter.hpp"

namespace Libs::Image
{
void GenerateIrradianceCubemap(float* source, int width, int height, float*& output)
{
    output = nullptr;
    Gfx::ImageDescription imgDesc{};
    imgDesc.width = width;
    imgDesc.height = height;
    imgDesc.depth = 1;
    imgDesc.isCubemap = false;
    imgDesc.mipLevels = 1;
    imgDesc.format = Gfx::ImageFormat::R32G32B32A32_SFloat;
    imgDesc.depth = 1;
    imgDesc.multiSampling = Gfx::MultiSampling::Sample_Count_1;

    std::unique_ptr<Gfx::Image> srcImage = GetGfxDriver()->CreateImage(
        imgDesc,
        Gfx::ImageUsage::TransferDst | Gfx::ImageUsage::Texture | Gfx::ImageUsage::Storage
    );
    std::unique_ptr<Gfx::Buffer> sourceBuf =
        GetGfxDriver()->CreateBuffer(srcImage->GetDescription().GetByteSize(), Gfx::BufferUsage::Transfer_Src, true);
    memcpy(sourceBuf->GetCPUVisibleAddress(), source, width * height * 4 * sizeof(float));

    const int irradianceMapSize = 1024;
    imgDesc.width = irradianceMapSize;
    imgDesc.height = irradianceMapSize;
    imgDesc.isCubemap = true;
    std::unique_ptr<Gfx::Image> dstCuebmap = GetGfxDriver()->CreateImage(
        imgDesc,
        Gfx::ImageUsage::Texture | Gfx::ImageUsage::Storage | Gfx::ImageUsage::TransferSrc
    );

    auto cmd = GetGfxDriver()->CreateCommandBuffer();
    ComputeShader* compute = (ComputeShader*)AssetDatabase::Singleton()->LoadAsset(
        "_engine_internal/Shaders/Utils/IrradianceMapGeneration.comp"
    );

    Gfx::BufferImageCopyRegion srcCopy[] = {
        {0,
         {Gfx::ImageAspect::Color, 0, 0, 1},
         {0, 0, 0},
         {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1}}
    };
    cmd->CopyBufferToImage(sourceBuf, srcImage, srcCopy);
    cmd->SetTexture("_Src", *srcImage);
    cmd->SetTexture("_Dst", *dstCuebmap);
    cmd->BindShaderProgram(compute->GetDefaultShaderProgram(), compute->GetDefaultShaderConfig());
    cmd->Dispatch(irradianceMapSize / 8, irradianceMapSize / 8, 6);

    auto readbackBuf = GetGfxDriver()->CreateBuffer(imgDesc.GetByteSize(), Gfx::BufferUsage::Transfer_Dst, true);
    // copy processed image to cpu
    Gfx::BufferImageCopyRegion regions[] = {
        {.srcOffset = 0,
         .layers = {Gfx::ImageAspect::Color, 0, 0, 6},
         .offset = {0, 0, 0},
         .extend = {irradianceMapSize, irradianceMapSize, 1}}
    };
    cmd->CopyImageToBuffer(dstCuebmap, readbackBuf, regions);

    GetGfxDriver()->ExecuteCommandBufferImmediately(*cmd);

    size_t readbackSize = irradianceMapSize * irradianceMapSize * 4 * 6;
    float* readback = new float[readbackSize];
    memcpy(readback, readbackBuf->GetCPUVisibleAddress(), readbackSize * sizeof(float));

    Exporters::KtxExporter::Export(
        (AssetDatabase::Singleton()->GetProjectRoot() / "Assets/store.ktx").string().c_str(),
        (uint8_t*)readback,
        irradianceMapSize,
        irradianceMapSize,
        1,
        2,
        1,
        1,
        false,
        true,
        Gfx::ImageFormat::R32G32B32A32_SFloat
    );

    // size_t oneFaceSize4 = irradianceMapSize * irradianceMapSize * 4;
    //
    // for (int i = 0; i < 6; ++i)
    // {
    //     stbi_write_hdr(
    //         (std::string("store") + std::to_string(i) + ".hdr").c_str(),
    //         irradianceMapSize,
    //         irradianceMapSize,
    //         4,
    //         (readback + oneFaceSize4 * i)
    //     );
    // }
    delete[] readback;
}
} // namespace Libs::Image
