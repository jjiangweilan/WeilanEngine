#include "TextureLoader.hpp"
#include "ThirdParty/stb/stb_image.h"
#include <ktx.h>
#include <ktxvulkan.h>
#include "Libs/Image/ImageProcessing.hpp"

bool TextureLoader::ImportNeeded()
{
    size_t oldLastWriteTime = meta["lastWriteTime"];
    size_t lastWriteTime = std::filesystem::last_write_time(absoluteAssetPath).time_since_epoch().count();
    return lastWriteTime > oldLastWriteTime;
}

void TextureLoader::Import()
{
    nlohmann::json option = meta["importOption"];
    bool generateMipmap = option["generateMipmap"];
}
void TextureLoader::Load()
{
    std::string importedSourcePath = meta["importedSourcePath"];

    uint8_t* sourceBinary = ImportDatabase::Singleton()->ReadFile(importedSourcePath);
}

void TextureLoader::GetReferenceResolveData(Serializer*& serializer, SerializeReferenceResolveMap*& resolveMap) {}
std::unique_ptr<Asset> TextureLoader::RetrieveAsset() {}

void TextureLoader::LoadStbSupoprtedTexture(uint8_t* data, size_t byteSize)
{
    int width, height, channels, desiredChannels;
    stbi_info_from_memory(data, byteSize, &width, &height, &desiredChannels);
    if (desiredChannels == 3) // 3 channel srgb texture is not supported on PC
        desiredChannels = 4;

    bool is16Bit = stbi_is_16_bit_from_memory(data, byteSize);
    bool isHDR = stbi_is_hdr_from_memory(data, byteSize);

    stbi_uc* loaded = nullptr;
    if (isHDR)
    {
        loaded = (stbi_uc*)stbi_loadf_from_memory(data, (int)byteSize, &width, &height, &channels, desiredChannels);
    }
    else
    {
        loaded = stbi_load_from_memory(data, (int)byteSize, &width, &height, &channels, desiredChannels);
    }

    int mipLevels = glm::floor(glm::log2((float)glm::min(width, height))) + 1;

    size_t mippedDataByteSize = 0;
    size_t elementSize = 0;
    if (isHDR)
    {
        float* data;
        elementSize = sizeof(float);
        Libs::Image::GenerateBoxFilteredMipmap<float>(
            (float*)loaded,
            width,
            height,
            mipLevels,
            desiredChannels,
            data,
            mippedDataByteSize
        );
        stbi_image_free(loaded);
    }
    else if (is16Bit)
    {
        throw std::exception("Not implemented");
    }
    else
    {
        uint8_t* data;
        size_t s = texDesc.img.GetByteSize();

        elementSize = sizeof(uint8_t);
        Libs::Image::GenerateBoxFilteredMipmap<uint8_t>(
            (uint8_t*)loaded,
            width,
            height,
            (int)texDesc.img.mipLevels,
            desiredChannels,
            data,
            mippedDataByteSize
        );
        texDesc.data = (uint8_t*)data;
        stbi_image_free(loaded);
    }

    Gfx::ImageFormat format = Gfx::ImageFormat::R8G8B8A8_SRGB;
    if (desiredChannels == 4)
    {
        if (isHDR)
            format = Gfx::ImageFormat::R32G32B32A32_SFloat;
        else if (is16Bit)
            format = Gfx::ImageFormat::R16G16B16A16_SFloat;
        else
            format = Gfx::ImageFormat::R8G8B8A8_SRGB;
    }
    else if (desiredChannels == 3)
    {
        if (isHDR)
            format = Gfx::ImageFormat::R32G32B32_SFloat;
        else if (is16Bit)
            format = Gfx::ImageFormat::R16G16B16_SFloat;
        else
            format = Gfx::ImageFormat::R8G8B8_SRGB;
    }
    else if (desiredChannels == 2)
    {
        if (isHDR)
            format = Gfx::ImageFormat::R32G32_SFloat;
        else if (is16Bit)
            format = Gfx::ImageFormat::R16G16_SFloat;
        else
            format = Gfx::ImageFormat::R8G8_SRGB;
    }
    else if (desiredChannels == 1)
    {
        if (isHDR)
            format = Gfx::ImageFormat::R32_SFloat;
        else if (is16Bit)
            format = Gfx::ImageFormat::R16_SFloat;
        else
            format = Gfx::ImageFormat::R8_SRGB;
    }

    texDesc.img.format = format;
    desc = texDesc;

    image = GetGfxDriver()->CreateImage(
        desc.img,
        Gfx::ImageUsage::Texture | Gfx::ImageUsage::TransferSrc | Gfx::ImageUsage::TransferDst
    );

    int preLevelWidth = width;
    int preLevelHeight = height;
    int curLevelOffset = 0;
    for (uint32_t level = 0; level < desc.img.mipLevels; ++level)
    {
        float scale = std::pow(0.5f, level);
        int lw = preLevelWidth * scale;
        int lh = preLevelHeight * scale;
        size_t byteSize = lw * lh * desiredChannels * elementSize;

        GetGfxDriver()->UploadImage(*image, desc.data + curLevelOffset, byteSize, level, 0);

        curLevelOffset += byteSize;
        preLevelWidth = lw;
        preLevelHeight = lh;
    }
    // ConvertRawImageToKtx(desc);
}
