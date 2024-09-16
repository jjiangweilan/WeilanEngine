#include "Texture.hpp"
#include "GfxDriver/GfxEnums.hpp"
#include "GfxDriver/Vulkan/Internal/VKEnumMapper.hpp"
#include "Libs/Image/ImageProcessing.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#define STB_IMAGE_IMPLEMENTATION
#include "ThirdParty/stb/stb_image.h"
#include <ktx.h>
#include <ktxvulkan.h>
#include <spdlog/spdlog.h>

// extensions comes from supported format in stb_image.h and ktx
DEFINE_ASSET(Texture, "01FD72D3-B18A-4182-95F1-81ECD3E5E6A8", "ktx,jpg,png,jpeg,bmp,hdr,psd,tga,gif,pic,pgm,ppm");

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

Texture::~Texture()
{
    if (desc.keepData && desc.data != nullptr)
    {
        delete[] desc.data;
    }
}

void Texture::CreateGfxImage(TextureDescription& texDesc)
{
    image = Gfx::GfxDriver::Instance()->CreateImage(
        texDesc.img,
        Gfx::ImageUsage::Texture | Gfx::ImageUsage::TransferDst | Gfx::ImageUsage::TransferSrc
    );
    image->SetName(GetName());

    size_t byteSize = Gfx::MapImageFormatToByteSize(texDesc.img.format) * texDesc.img.width * texDesc.img.height;
    uint8_t* data = texDesc.data;

    GetGfxDriver()->UploadImage(*image, data, byteSize);
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

void Texture::LoadKtxTexture(ktxTexture2* texture, int gpuMipLevels)
{
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
                        std::string message = "Vulkan implementation does not support any available transcode target.";
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
                        std::string message = "Vulkan implementation does not support any available transcode target.";
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
    desc.img.mipLevels = gpuMipLevels;
    desc.img.format = Gfx::MapVKFormat(ktxTexture2_GetVkFormat(texture));
    desc.img.multiSampling = Gfx::MultiSampling::Sample_Count_1;
    desc.data = nullptr;

    image = Gfx::GfxDriver::Instance()->CreateImage(desc.img, Gfx::ImageUsage::Texture | Gfx::ImageUsage::TransferDst);

    ktx_uint8_t* data = ktxTexture_GetData(ktxTexture(texture));
    size_t byteSize = ktxTexture_GetDataSize(ktxTexture(texture));
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
                    if (ktxTexture_GetImageOffset(ktxTexture(texture), level, layer, face, &offset) != KTX_SUCCESS)
                        throw std::runtime_error("Texture-failed to get image offset");
                    ktx_size_t byteSize = ktxTexture_GetImageSize(ktxTexture(texture), level);

                    GetGfxDriver()->UploadImage(*image, data + offset, byteSize, level, layer + face);
                }
            }
        }
    }
    else
    {
        std::runtime_error("Texture-numDimensions not implemented");
    }
}

void Texture::LoadKtxTexture(uint8_t* imageData, size_t imageByteSize)
{
    if (IsKTX1File(imageData) || IsKTX2File(imageData))
    {
        ktxTexture2* texture;
        KTX_error_code e = ktxTexture2_CreateFromMemory(
            (uint8_t*)imageData,
            imageByteSize,
            KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
            &texture
        );
        if (e != KTX_SUCCESS)
        {
            throw std::runtime_error("Texture-Failed to create texture from memory");
        }

        LoadKtxTexture(texture, texture->numLevels);
        ktxTexture_Destroy(ktxTexture(texture)); // https://github.khronos.org/KTX-Software/libktx/index.html#readktx
    }
}

// TODO: this is a transmission step, the generation of ktx texture should be a importing step
void Texture::ConvertRawImageToKtx(TextureDescription& desc)
{
    ktxTexture2* texture;
    ktxTextureCreateInfo createInfo;
    KTX_error_code result;
    ktx_uint32_t level, layer, faceSlice;
    uint8_t* src;
    ktx_size_t srcSize;

    createInfo.glInternalformat = 0; // Ignored as we'll create a KTX2 texture.
    createInfo.vkFormat = Gfx::MapFormat(desc.img.format);
    createInfo.baseWidth = desc.img.width;
    createInfo.baseHeight = desc.img.height;
    createInfo.baseDepth = desc.img.depth;
    createInfo.numDimensions = 2;
    // Note: it is not necessary to provide a full mipmap pyramid.
    createInfo.numLevels = 1;
    createInfo.numFaces = desc.img.isCubemap ? 6 : 1;
    createInfo.isArray = KTX_FALSE;
    createInfo.generateMipmaps = KTX_FALSE;
    createInfo.numLayers = 1;

    result = ktxTexture2_Create(&createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &texture);
    if (result != KTX_SUCCESS)
    {
        spdlog::error("failed to create ktx texture");
        return;
    }

    src = desc.data;
    srcSize = desc.img.GetByteSize();
    level = 0;
    layer = 0;
    faceSlice = 0;
    result = ktxTexture_SetImageFromMemory(ktxTexture(texture), level, layer, faceSlice, src, srcSize);
    if (result != KTX_SUCCESS)
    {
        spdlog::error("failed to create ktx texture");
        return;
    }
    // Repeat for the other 15 slices of the base level and all other levels
    // up to createInfo.numLevels.

    ktxBasisParams params = {0};
    params.structSize = sizeof(params);
    // For BasisLZ/ETC1S
    params.compressionLevel = KTX_ETC1S_DEFAULT_COMPRESSION_LEVEL;
    // For UASTC
    params.uastc = KTX_TRUE;
    // Set other BasisLZ/ETC1S or UASTC params to change default quality settings.
    result = ktxTexture2_CompressBasisEx(texture, &params);
    if (result != KTX_SUCCESS)
    {
        spdlog::error("failed to create ktx texture");
        return;
    }
    char writer[100];
    snprintf(writer, sizeof(writer), "%s version %s", "WeilanEngine", 0);
    ktxHashList_AddKVPair(&texture->kvDataHead, KTX_WRITER_KEY, (ktx_uint32_t)strlen(writer) + 1, writer);

    LoadKtxTexture(texture, desc.img.mipLevels);
    GetGfxDriver()->GenerateMipmaps(*image);

    ktxTexture_Destroy(ktxTexture(texture));
}

void Texture::LoadStbSupoprtedTexture(uint8_t* data, size_t byteSize)
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

    TextureDescription texDesc{};
    texDesc.img.width = width;
    texDesc.img.height = height;
    texDesc.img.depth = 1;
    texDesc.img.mipLevels = glm::floor(glm::log2((float)glm::min(width, height))) + 1;
    texDesc.img.multiSampling = Gfx::MultiSampling::Sample_Count_1;
    texDesc.img.isCubemap = false;

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
            (int)texDesc.img.mipLevels,
            desiredChannels,
            data,
            mippedDataByteSize
        );
        texDesc.data = (uint8_t*)data;
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
    GetGfxDriver()->GenerateMipmaps(*image);

    // ConvertRawImageToKtx(desc);
}

void Texture::SaveAsCubemap(const char* filename)
{
    auto fpath = sourceAssetFile;

    if (fpath.has_extension())
    {
        std::fstream f;
        f.open(fpath, std::ios::binary | std::ios_base::in);
        if (f.good() && f.is_open())
        {
            std::stringstream ss;
            ss << f.rdbuf();
            std::string s = ss.str();

            auto ext = fpath.extension();
            if (ext == ".ktx")
            {
                spdlog::error("not implemented");
                return;
            }
            else
            {
                auto& exts = Texture::StaticGetExtensions();
                for (auto& e : exts)
                {
                    if (e != ".ktx" && e == ext)
                    {
                        uint8_t* data = (uint8_t*)s.data();
                        size_t byteSize = s.size();

                        int width, height, channels, desiredChannels;
                        stbi_info_from_memory(data, byteSize, &width, &height, &desiredChannels);
                        if (desiredChannels == 3) // 3 channel srgb texture is not supported on PC
                            desiredChannels = 4;

                        bool isHDR = stbi_is_hdr_from_memory(data, byteSize);

                        stbi_uc* loaded = nullptr;
                        if (isHDR)
                        {
                            loaded = (stbi_uc*)stbi_loadf_from_memory(
                                data,
                                (int)byteSize,
                                &width,
                                &height,
                                &channels,
                                desiredChannels
                            );

                            float* d;
                            Libs::Image::GenerateIrradianceCubemap((float*)loaded, width, height, d);
                        }
                        break;
                    }
                }
            }
        }
    }
}

bool Texture::LoadFromFile(const char* path)
{
    sourceAssetFile = path;
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
            else
            {
                auto& exts = Texture::StaticGetExtensions();
                for (auto& e : exts)
                {
                    if (e != ".ktx" && e == ext)
                    {
                        LoadStbSupoprtedTexture((uint8_t*)s.data(), s.size());
                        break;
                    }
                }
            }
        }
        else
        {
            return false;
        }
    }

    return true;
}
