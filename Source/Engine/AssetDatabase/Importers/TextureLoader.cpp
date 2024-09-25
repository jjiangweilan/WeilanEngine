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
    bool reimport = lastWriteTime > oldLastWriteTime;

    // meta file validation
    if (meta.contains("importedKtxFile"))
    {
        reimport = reimport || !std::filesystem::exists(meta["importedKtxFile"]);
    }
    else
        reimport = true;

    return reimport;
}

void TextureLoader::Import()
{
    std::string importFileUUID = meta.value("importFileUUID", UUID().ToString());
    nlohmann::json option = meta.value("importOption", nlohmann::json::object_t{});
    bool generateMipmap = option.value("generateMipmap", false);
    bool converToCubemap = option.value("converToCubemap", false);
    auto importedAssetPath = importDatabase->GetImportAssetPath(importFileUUID).replace_extension(".ktx");

    std::fstream f;
    f.open(absoluteAssetPath, std::ios::binary | std::ios_base::in);
    if (f.good() && f.is_open())
    {
        if (absoluteAssetPath.extension() != ".ktx")
        {
            std::stringstream ss;
            ss << f.rdbuf();
            std::string s = ss.str();

            uint8_t* data = (uint8_t*)s.data();
            size_t byteSize = s.size();
            int width, height, channels, desiredChannels;
            int layers = converToCubemap ? 6 : 1;
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
                loaded =
                    (stbi_uc*)stbi_loadf_from_memory(data, (int)byteSize, &width, &height, &channels, desiredChannels);
            }
            else
            {
                loaded = stbi_load_from_memory(data, (int)byteSize, &width, &height, &channels, desiredChannels);
            }

            if (converToCubemap)
            {
                uint8_t* output;
                int cubemapSize = 1024;
                Libs::Image::GenerateIrradianceCubemap((float*)loaded, width, height, cubemapSize, output);
                delete[] loaded;
                loaded = output;
                width = cubemapSize;
                height = cubemapSize;
            }

            if (generateMipmap)
            {
                size_t mippedDataByteSize = 0;
                uint8_t* mippedData = nullptr;
                if (isHDR)
                {
                    Libs::Image::GenerateBoxFilteredMipmap<float>(
                        loaded,
                        width,
                        height,
                        layers,
                        mipLevels,
                        desiredChannels,
                        mippedData,
                        mippedDataByteSize
                    );
                    stbi_image_free(loaded);
                    loaded = mippedData;
                }
                else if (is16Bit)
                {
                    throw std::exception("Not implemented");
                }
                else
                {
                    Libs::Image::GenerateBoxFilteredMipmap<uint8_t>(
                        loaded,
                        width,
                        height,
                        layers,
                        mipLevels,
                        desiredChannels,
                        mippedData,
                        mippedDataByteSize
                    );
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

            //
            // note: this "converToCube ? 1 : layers" prevents creating array of cubeMaps, but ktx separate the
            // concept of face and layer, that's why I need to manually convert layer to face
            Exporters::KtxExporter::Export(
                importedAssetPath.string().c_str(),
                loaded,
                width,
                height,
                1,
                2,
                converToCubemap ? 1 : layers,
                mipLevels,
                false,
                converToCubemap,
                format
            );

            delete[] loaded;
        }
        else
        {
            std::ofstream outf;
            outf.open(importedAssetPath, std::ios::trunc | std::ios::out | std::ios::binary);
            if (outf.good() && f.is_open())
            {
                outf << f.rdbuf();
            }
        }
    }
    meta["lastImportedWriteTime"] = std::filesystem::last_write_time(absoluteAssetPath).time_since_epoch().count();
    meta["importedKtxFile"] = importedAssetPath.string();
    meta["importFileUUID"] = importFileUUID;
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
    std::string importedSourcePath = meta["importedKtxFile"];

    auto sourceBinaryVec = importDatabase->ReadFile(importedSourcePath);
    uint8_t* sourceBinary = sourceBinaryVec.data();
    size_t binarySize = sourceBinaryVec.size();

    this->texture = std::make_unique<Texture>(KtxTexture{sourceBinary, binarySize});
}
