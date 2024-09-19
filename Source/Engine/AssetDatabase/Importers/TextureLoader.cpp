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

bool TextureLoader::ImportNeeded()
{
    size_t oldLastWriteTime = meta["lastImportedWriteTime"];
    size_t lastWriteTime = std::filesystem::last_write_time(absoluteAssetPath).time_since_epoch().count();
    return lastWriteTime > oldLastWriteTime;
}

void TextureLoader::Import()
{
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

        UUID importUUID;
        auto importedAssetPath = ImportDatabase::Singleton().GetImportAssetPath(importUUID.ToString());
        Exporters::KtxExporter::
            Export(importedAssetPath.string().c_str(), loaded, width, height, 1, 2, 1, mipLevels, false, false, format);

        meta["lastImportedWriteTime"] = std::filesystem::last_write_time(absoluteAssetPath).time_since_epoch().count();
        meta["improtedKtxFile"] = importedAssetPath.string();
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

    if (IsKTX2File(sourceBinary))
    {
        ktxTexture2* texture;
        KTX_error_code e =
            ktxTexture2_CreateFromMemory(sourceBinary, binarySize, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture);
        if (e != KTX_SUCCESS)
        {
            throw std::runtime_error("Texture-Failed to create texture from memory");
        }

        if (ktxTexture2_NeedsTranscoding(texture))
        {
            enum class CompressionMode
            {
                UASTC,
                ETC1S
            } compressionMode;
            if (KHR_DFDVAL(texture->pDfd + 1, MODEL) == KHR_DF_MODEL_ETC1S)
                compressionMode = CompressionMode::ETC1S;
            else if (KHR_DFDVAL(texture->pDfd + 1, MODEL) == KHR_DF_MODEL_UASTC)
                compressionMode = CompressionMode::UASTC;
            else
            {
                throw std::runtime_error("Texture-failed to create ktx texture");
            }

            ktx_texture_transcode_fmt_e tf;
            auto& gpuFeatures = GetGfxDriver()->GetGPUFeatures();
            switch (compressionMode)
            {
                case CompressionMode::ETC1S:
                    {
                        if (gpuFeatures.textureCompressionETC2)
                            tf = KTX_TTF_ETC2_RGBA;
                        else if (gpuFeatures.textureCompressionBC)
                            tf = KTX_TTF_BC3_RGBA;
                        else
                        {
                            std::string message =
                                "Vulkan implementation does not support any available transcode target.";
                            throw std::runtime_error(message);
                        }
                        break;
                    }
                case CompressionMode::UASTC:
                    {
                        if (gpuFeatures.textureCompressionASTC4x4)
                            tf = KTX_TTF_ASTC_4x4_RGBA;
                        else if (gpuFeatures.textureCompressionBC)
                            tf = KTX_TTF_BC7_RGBA;
                        else
                        {
                            std::string message =
                                "Vulkan implementation does not support any available transcode target.";
                            throw std::runtime_error(message);
                        }
                        break;
                    }
            }

            if (ktxTexture2_TranscodeBasis((ktxTexture2*)texture, tf, 0) != 0)
            {
                throw std::runtime_error("failed to transcode ktx texture");
            }
        }

        ktx_uint8_t* data = ktxTexture_GetData(ktxTexture(texture));
        size_t byteSize = ktxTexture_GetDataSize(ktxTexture(texture));
        this->texture = std::make_unique<Texture>(KtxTexture{data, byteSize});

        ktxTexture_Destroy(ktxTexture(texture)); // https://github.khronos.org/KTX-Software/libktx/index.html#readktx
    }
}
