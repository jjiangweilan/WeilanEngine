#include "TextureLoader.hpp"
#include "AssetDatabase/Exporters/KtxExporter.hpp"
#include "Core/Texture.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "GfxDriver/Image.hpp"
#include "GfxDriver/Vulkan/Internal/VKEnumMapper.hpp"
#include "Libs/Image/ImageProcessing.hpp"
#include "ThirdParty/stb/stb_image.h"
#include <fstream>
#include <ktx.h>
#include <ktxvulkan.h>

DEFINE_ASSET_LOADER(TextureLoader, "ktx,jpg,png,jpeg,bmp,hdr,psd,tga,gif,pic,pgm,ppm")

bool TextureLoader::ImportNeeded()
{
    size_t oldLastWriteTime = meta.value("lastImportedWriteTime", 0ll);
    size_t lastWriteTime = std::filesystem::last_write_time(absoluteAssetPath).time_since_epoch().count();
    return lastWriteTime > oldLastWriteTime;
}

void TextureLoader::Import()
{
    std::string uuid = meta.value("uuid", UUID().ToString());
    nlohmann::json option = meta.value("importOption", nlohmann::json::object_t{});
    bool generateMipmap = option.value("generateMipmap", false);

    std::fstream f;
    f.open(absoluteAssetPath, std::ios::binary | std::ios_base::in);
    if (f.good() && f.is_open())
    {
        std::stringstream ss;
        ss << f.rdbuf();
        std::string s = ss.str();

        uint8_t* data = (uint8_t*)s.data();
        size_t byteSize = s.size();
        int width, height, channels, desiredChannels;
        stbi_info_from_memory(data, byteSize, &width, &height, &desiredChannels);
        if (desiredChannels == 3) // 3 channel srgb texture is not supported on PC
            desiredChannels = 4;
        // image information
        bool is16Bit = stbi_is_16_bit_from_memory(data, byteSize);
        bool isHDR = stbi_is_hdr_from_memory(data, byteSize);
        int mipLevels = generateMipmap ? glm::floor(glm::log2((float)glm::min(width, height))) + 1 : 1;

        uint8_t* loaded = nullptr;
        if (isHDR)
        {
            loaded = (stbi_uc*)stbi_loadf_from_memory(data, (int)byteSize, &width, &height, &channels, desiredChannels);
        }
        else
        {
            loaded = stbi_load_from_memory(data, (int)byteSize, &width, &height, &channels, desiredChannels);
        }

        if (generateMipmap)
        {
            size_t mippedDataByteSize = 0;
            uint8_t* mippedData = nullptr;
            if (isHDR)
            {
                Libs::Image::GenerateBoxFilteredMipmap<
                    float>(loaded, width, height, mipLevels, desiredChannels, mippedData, mippedDataByteSize);
                stbi_image_free(loaded);
                loaded = mippedData;
            }
            else if (is16Bit)
            {
                throw std::exception("Not implemented");
            }
            else
            {
                Libs::Image::GenerateBoxFilteredMipmap<
                    uint8_t>(loaded, width, height, mipLevels, desiredChannels, mippedData, mippedDataByteSize);
                stbi_image_free(loaded);
                loaded = mippedData;
            }
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

        auto importedAssetPath = ImportDatabase::Singleton().GetImportAssetPath(uuid);
        Exporters::KtxExporter::
            Export(importedAssetPath.string().c_str(), loaded, width, height, 1, 2, 1, mipLevels, false, false, format);

        meta["lastImportedWriteTime"] = std::filesystem::last_write_time(absoluteAssetPath).time_since_epoch().count();
        meta["improtedKtxFile"] = importedAssetPath.string();
        meta["uuid"] = uuid;
    }
}

bool TextureLoader::IsKTX2File(ktx_uint8_t* imageData)
{
    return imageData[0] == 0xAB && imageData[1] == 0x4B && imageData[2] == 0x54 && imageData[3] == 0x58 &&
           imageData[4] == 0x20 && imageData[5] == 0x32 && imageData[6] == 0x30 && imageData[7] == 0xBB &&
           imageData[8] == 0x0D && imageData[9] == 0x0A && imageData[10] == 0x1A && imageData[11] == 0x0A;
}

bool TextureLoader::IsKTX1File(ktx_uint8_t* imageData)
{
    return imageData[0] == 0xAB && imageData[1] == 0x4B && imageData[2] == 0x54 && imageData[3] == 0x58 &&
           imageData[4] == 0x20 && imageData[5] == 0x31 && imageData[6] == 0x31 && imageData[7] == 0xBB &&
           imageData[8] == 0x0D && imageData[9] == 0x0A && imageData[10] == 0x1A && imageData[110] == 0x0A;
}

void TextureLoader::Load()
{
    std::string importedSourcePath = meta["improtedKtxFile"];

    auto sourceBinaryVec = ImportDatabase::Singleton().ReadFile(importedSourcePath);
    uint8_t* sourceBinary = sourceBinaryVec.data();
    size_t binarySize = sourceBinaryVec.size();

    this->texture = std::make_unique<Texture>(KtxTexture{sourceBinary, binarySize});
}
