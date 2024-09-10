#include "Texture.hpp"
#include "GfxDriver/GfxEnums.hpp"
#include "GfxDriver/Vulkan/Internal/VKEnumMapper.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#define STB_IMAGE_IMPLEMENTATION
#include "ThirdParty/stb/stb_image.h"
#include <ktxvulkan.h>
#include <spdlog/spdlog.h>


DEFINE_ASSET(Texture, "01FD72D3-B18A-4182-95F1-81ECD3E5E6A8", "ktx,jpg");

Texture::Texture(const char* path, const UUID& uuid)
{
    SetUUID(uuid);
    if (!LoadFromFile(path))
    {
        throw std::runtime_error("Failed to create texture");
    }
}

Texture::Texture(TextureDescription texDesc, const UUID& uuid) : desc(texDesc)
{
    SetUUID(uuid);

    CreateGfxImage(desc);
}

Texture::Texture(uint8_t* data, size_t byteSize, ImageDataType imageDataType, const UUID& uuid)
{
    SetUUID(uuid);

    switch (imageDataType)
    {
        case ImageDataType::Ktx: LoadKtxTexture(data, byteSize); break;
        case ImageDataType::StbSupported: LoadStbSupoprtedTexture(data, byteSize); break;
    }
}

void Texture::CreateGfxImage(TextureDescription& texDesc)
{
    image = Gfx::GfxDriver::Instance()->CreateImage(
        texDesc.img,
        Gfx::ImageUsage::Texture | Gfx::ImageUsage::TransferDst | Gfx::ImageUsage::TransferSrc
    );
    image->SetName(GetName());

    uint32_t byteSize = Gfx::MapImageFormatToByteSize(texDesc.img.format) * texDesc.img.width * texDesc.img.height;
    // Gfx::Buffer::CreateInfo bufCreateInfo;
    // bufCreateInfo.size = byteSize;
    // bufCreateInfo.usages = Gfx::BufferUsage::Transfer_Src;
    // bufCreateInfo.debugName = "mesh staging buffer";
    // bufCreateInfo.visibleInCPU = true;
    // std::unique_ptr<Gfx::Buffer> stagingBuffer = Gfx::GfxDriver::Instance()->CreateBuffer(bufCreateInfo);
    // memcpy(stagingBuffer->GetCPUVisibleAddress(), texDesc.data, byteSize);

    GetGfxDriver()->UploadImage(*image, texDesc.data, byteSize);
    GetGfxDriver()->GenerateMipmaps(*image);

    // mip map generation
}

bool IsKTX2File(ktx_uint8_t* imageData)
{
    return imageData[0] == 0xAB && imageData[1] == 0x4B && imageData[2] == 0x54 && imageData[3] == 0x58 &&
           imageData[4] == 0x20 && imageData[5] == 0x32 && imageData[6] == 0x30 && imageData[7] == 0xBB &&
           imageData[8] == 0x0D && imageData[9] == 0x0A && imageData[10] == 0x1A && imageData[11] == 0x0A;
}

bool IsKTX1File(ktx_uint8_t* imageData)
{
    return imageData[0] == 0xAB && imageData[1] == 0x4B && imageData[2] == 0x54 && imageData[3] == 0x58 &&
           imageData[4] == 0x20 && imageData[5] == 0x31 && imageData[6] == 0x31 && imageData[7] == 0xBB &&
           imageData[8] == 0x0D && imageData[9] == 0x0A && imageData[10] == 0x1A && imageData[110] == 0x0A;
}

Texture::Texture(KtxTexture texDesc, const UUID& uuid)
{
    SetUUID(uuid);
    ktx_uint8_t* imageData = texDesc.imageData;
    uint32_t imageByteSize = texDesc.byteSize;
    LoadKtxTexture(imageData, imageByteSize);
}

void Texture::Reload(Asset&& loaded)
{
    Texture* newTex = static_cast<Texture*>(&loaded);
    image = std::move(newTex->image);
    Asset::Reload(std::move(loaded));
}

void Texture::LoadKtxTexture(uint8_t* imageData, size_t imageByteSize)
{
    if (IsKTX1File(imageData) || IsKTX2File(imageData))
    {
        ktxTexture* texture;
        if (ktxTexture_CreateFromMemory(
                (uint8_t*)imageData,
                imageByteSize,
                KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
                &texture
            ) != KTX_SUCCESS)
        {
            throw std::runtime_error("Texture-Failed to create texture from memory");
        }

        if (ktxTexture_NeedsTranscoding(texture))
        {
            char* writerScParams;
            uint32_t valueLen;
            enum class CompressionMode
            {
                UASTC,
                ETC1S
            } compressionMode;
            if (KTX_SUCCESS == ktxHashList_FindValue(
                                   &texture->kvDataHead,
                                   KTX_WRITER_SCPARAMS_KEY,
                                   &valueLen,
                                   (void**)&writerScParams
                               ))
            {
                if (std::strstr(writerScParams, "--uastc"))
                    compressionMode = CompressionMode::UASTC;
                else
                    compressionMode = CompressionMode::ETC1S;
            }
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

        desc.img.isCubemap = texture->isCubemap;
        desc.img.width = texture->baseWidth;
        desc.img.height = texture->baseHeight;
        desc.img.mipLevels = texture->numLevels;
        desc.img.format = Gfx::MapVKFormat(ktxTexture_GetVkFormat(texture));
        desc.img.multiSampling = Gfx::MultiSampling::Sample_Count_1;
        desc.data = nullptr;

        image =
            Gfx::GfxDriver::Instance()->CreateImage(desc.img, Gfx::ImageUsage::Texture | Gfx::ImageUsage::TransferDst);

        ktx_uint8_t* data = ktxTexture_GetData(texture);
        size_t byteSize = ktxTexture_GetDataSize(texture);
        if (texture->numDimensions == 1)
        {
            throw std::runtime_error("Texture-numDimensions not implemented");
        }
        else if (texture->numDimensions == 2)
        {
            for (uint32_t level = 0; level < texture->numLevels; ++level)
            {
                for (uint32_t layer = 0; layer < texture->numLayers; ++layer)
                {
                    for (uint32_t face = 0; face < texture->numFaces; ++face)
                    {
                        ktx_size_t offset = 0;
                        if (ktxTexture_GetImageOffset(texture, level, layer, face, &offset) != KTX_SUCCESS)
                            throw std::runtime_error("Texture-failed to get image offset");
                        ktx_size_t byteSize = ktxTexture_GetImageSize(texture, level);

                        GetGfxDriver()->UploadImage(*image, data + offset, byteSize, level, layer + face);
                    }
                }
            }
        }
        else
        {
            std::runtime_error("Texture-numDimensions not implemented");
        }

        ktxTexture_Destroy(texture); // https://github.khronos.org/KTX-Software/libktx/index.html#readktx
    }
}

void Texture::LoadStbSupoprtedTexture(uint8_t* data, size_t byteSize)
{
    int width, height, channels, desiredChannels;
    stbi_info_from_memory(data, byteSize, &width, &height, &desiredChannels);
    if (desiredChannels == 3) // 3 channel srgb texture is not supported on PC
        desiredChannels = 4;

    stbi_uc* loaded = stbi_load_from_memory(data, (int)byteSize, &width, &height, &channels, desiredChannels);

    bool is16Bit = stbi_is_16_bit_from_memory(data, byteSize);
    bool isHDR = stbi_is_hdr_from_memory(data, byteSize);
    if (is16Bit && isHDR)
    {
        SPDLOG_ERROR("16 bits and hdr texture Not Implemented");
    }

    TextureDescription texDesc{};
    texDesc.img.width = width;
    texDesc.img.height = height;
    texDesc.img.depth = 1;
    texDesc.img.mipLevels = glm::floor(glm::log2((float)glm::max(width, height))) + 1;
    texDesc.img.multiSampling = Gfx::MultiSampling::Sample_Count_1;
    texDesc.img.isCubemap = false;
    texDesc.data = loaded;

    if (desiredChannels == 4)
    {
        texDesc.img.format = Gfx::ImageFormat::R8G8B8A8_SRGB;
    }
    if (desiredChannels == 3)
    {
        texDesc.img.format = Gfx::ImageFormat::R8G8B8_SRGB;
    }
    if (desiredChannels == 2)
    {
        texDesc.img.format = Gfx::ImageFormat::R8G8_SRGB;
    }
    if (desiredChannels == 1)
    {
        texDesc.img.format = Gfx::ImageFormat::R8_SRGB;
    }
    desc = texDesc;

    CreateGfxImage(desc);
}

bool Texture::LoadFromFile(const char* path)
{
    std::filesystem::path fpath(path);

    if (fpath.has_extension())
    {
        std::fstream f;
        f.open(path, std::ios::binary | std::ios_base::in);
        if (f.good() && f.is_open())
        {
            std::stringstream ss;
            ss << f.rdbuf();
            std::string s = ss.str();

            auto ext = fpath.extension();
            if (ext == ".ktx")
            {
                LoadKtxTexture((uint8_t*)s.data(), s.size());
            }
            else if (ext == ".jpg" || ext == ".png")
            {
                LoadStbSupoprtedTexture((uint8_t*)s.data(), s.size());
            }
        }
        else
        {
            return false;
        }
    }

    return true;
}
