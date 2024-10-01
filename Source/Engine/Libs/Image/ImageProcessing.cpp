#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Texture.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Rendering/Material.hpp"
#include "Rendering/Shader.hpp"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "AssetDatabase/AssetDatabase.hpp"
#include "AssetDatabase/Exporters/KtxExporter.hpp"
#include "ThirdParty/stb/stb_image_write.h"

namespace Libs::Image
{
void GenerateIrradianceCubemap(float* source, int width, int height, int outputSize, uint8_t*& output)
{
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

    const uint32_t irradianceMapSize = static_cast<uint32_t>(outputSize);
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
    glm::vec4 texelSize = {1.0f / irradianceMapSize, 1.0f / irradianceMapSize, irradianceMapSize, irradianceMapSize};
    cmd->SetPushConstant(compute->GetDefaultShaderProgram(), &texelSize);
    cmd->BindShaderProgram(compute->GetDefaultShaderProgram(), compute->GetDefaultShaderConfig());
    cmd->Dispatch(irradianceMapSize / 8, irradianceMapSize / 8, 6);

    auto readbackBuf = GetGfxDriver()->CreateBuffer(imgDesc.GetByteSize(), Gfx::BufferUsage::Transfer_Dst, true);
    // copy processed image to cpu
    Gfx::BufferImageCopyRegion regions[] = {
        {.bufferOffset = 0,
         .layers = {Gfx::ImageAspect::Color, 0, 0, 6},
         .offset = {0, 0, 0},
         .extend = {irradianceMapSize, irradianceMapSize, 1}}
    };
    cmd->CopyImageToBuffer(dstCuebmap, readbackBuf, regions);

    GetGfxDriver()->ExecuteCommandBufferImmediately(*cmd);

    size_t readbackSize = irradianceMapSize * irradianceMapSize * 4 * 6 * sizeof(float);
    output = new uint8_t[readbackSize];
    memcpy(output, readbackBuf->GetCPUVisibleAddress(), readbackSize);
}

void GenerateReflectanceCubemap(float* source, int width, int height, int outputSize, uint8_t*& output, int& mipLevels)
{
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

    const uint32_t cubemapSize = static_cast<uint32_t>(outputSize);
    mipLevels = 5;
    imgDesc.width = cubemapSize;
    imgDesc.height = cubemapSize;
    imgDesc.isCubemap = true;
    imgDesc.mipLevels = mipLevels; // 5 roughness levels
    std::unique_ptr<Gfx::Image> dstCuebmap = GetGfxDriver()->CreateImage(
        imgDesc,
        Gfx::ImageUsage::Texture | Gfx::ImageUsage::Storage | Gfx::ImageUsage::TransferSrc
    );

    auto cmd = GetGfxDriver()->CreateCommandBuffer();
    ComputeShader* compute =
        (ComputeShader*)AssetDatabase::Singleton()->LoadAsset("_engine_internal/Shaders/Utils/IBLBRDF.comp");

    Gfx::BufferImageCopyRegion srcCopy[] = {
        {0,
         {Gfx::ImageAspect::Color, 0, 0, 1},
         {0, 0, 0},
         {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1}}
    };
    cmd->CopyBufferToImage(sourceBuf, srcImage, srcCopy);

    cmd->SetTexture("_EnvMap", *srcImage);

    std::vector<std::unique_ptr<Material>> mats;
    for (int mip = 0; mip < mipLevels; ++mip)
    {
        mats.push_back(std::make_unique<Material>(compute));
        Material* mat = mats.back().get();

        mat->SetTexture("_LightCubemap", dstCuebmap.get(), Gfx::ImageViewOption{mip, 1, 0, 6});
        struct PushConstant
        {
            glm::vec4 texelSize;
            float mip;
        } pc;

        int mipCubemapSize = cubemapSize * glm::pow(0.5, mip);
        pc.texelSize = {1.0f / mipCubemapSize, 1.0f / mipCubemapSize, mipCubemapSize, mipCubemapSize};
        pc.mip = mip;

        auto shaderProgram = compute->GetShaderProgram({"LIGHT_IBL"});
        cmd->BindResource(2, mat->GetShaderResource());
        cmd->SetPushConstant(shaderProgram, &pc);
        cmd->BindShaderProgram(shaderProgram, shaderProgram->GetDefaultShaderConfig());
        cmd->Dispatch(glm::ceil(mipCubemapSize / 8.0f), glm::ceil(mipCubemapSize / 8), 6);
    }

    auto readbackBuf = GetGfxDriver()->CreateBuffer(imgDesc.GetByteSize(), Gfx::BufferUsage::Transfer_Dst, true);
    // copy processed image to cpu
    uint32_t mipWidth = cubemapSize;
    uint32_t mipHeight = cubemapSize;
    size_t byteOffset = 0;
    for (uint32_t mip = 0; mip < mipLevels; ++mip)
    {
        Gfx::BufferImageCopyRegion regions[] = {
            {.bufferOffset = byteOffset,
             .layers = {Gfx::ImageAspect::Color, mip, 0, 6},
             .offset = {0, 0, 0},
             .extend = {static_cast<uint32_t>(mipWidth), static_cast<uint32_t>(mipHeight), 1}}
        };

        byteOffset += mipWidth * mipHeight * 6 * Gfx::MapImageFormatToByteSize(imgDesc.format);
        mipWidth *= 0.5;
        mipHeight *= 0.5;
        cmd->CopyImageToBuffer(dstCuebmap, readbackBuf, regions);
    }

    GetGfxDriver()->ExecuteCommandBufferImmediately(*cmd);

    size_t readbackSize = imgDesc.GetByteSize();
    output = new uint8_t[readbackSize];
    memcpy(output, readbackBuf->GetCPUVisibleAddress(), readbackSize);
}

glm::vec3 GetDirFromCubeUV(glm::vec2 uv, int faceIndex)
{
    uv = glm::vec2(2.0) * uv - glm::vec2(1.0); // Normalize UV to range [-1, 1]

    if (faceIndex == 0)
        return glm::vec3(1.0, -uv.y, -uv.x); // +X
    if (faceIndex == 1)
        return glm::vec3(-1.0, -uv.y, uv.x); // -X
    if (faceIndex == 2)
        return glm::vec3(uv.x, 1.0, uv.y); // +Y
    if (faceIndex == 3)
        return glm::vec3(uv.x, -1.0, -uv.y); // -Y
    if (faceIndex == 4)
        return glm::vec3(uv.x, -uv.y, 1.0); // +Z
    if (faceIndex == 5)
        return glm::vec3(-uv.x, -uv.y, -1.0); // -Z

    return glm::vec3(0.0);
}

glm::vec2 DirToEquirectangularUV(glm::vec3 dir)
{
    float samplePhi = glm::atan(dir.z, dir.x) + glm::pi<float>();
    float sampleTheta = glm::acos(dir.y);
    glm::vec2 srcUV = glm::vec2(samplePhi / glm::two_pi<float>(), sampleTheta / glm::pi<float>());
    return srcUV;
}
} // namespace Libs::Image
