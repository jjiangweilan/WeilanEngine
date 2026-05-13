#include "KtxExporter.hpp"
#include <Engine/Driver/GfxDriver/ImageDescription.hpp>
#include <Engine/Driver/GfxDriver/Vulkan/Internal/VKEnumMapper.hpp>
#include <ktx.h>
#include <spdlog/spdlog.h>

namespace Exporters
{
void KtxExporter::Export(
    const char* path,
    uint8_t* src,
    uint32_t width,
    uint32_t height,
    uint32_t depth,
    uint32_t dimension,
    uint32_t layer,
    uint32_t mipLevel,
    bool isArray,
    bool isCubemap,
    Gfx::GfxFormat format,
    bool enableCompression
)
{
    ktxTexture2* texture;
    ktxTextureCreateInfo createInfo{};
    KTX_error_code result;

    createInfo.glInternalformat = 0;
    createInfo.vkFormat = Gfx::MapFormat(format);
    createInfo.baseWidth = width;
    createInfo.baseHeight = height;
    createInfo.baseDepth = depth;
    createInfo.numDimensions = dimension;
    // Note: it is not necessary to provide a full mipmap pyramid.
    createInfo.numLevels = mipLevel;
    createInfo.numLayers = layer;
    createInfo.numFaces = isCubemap ? 6 : 1;
    createInfo.isArray = isArray;
    createInfo.generateMipmaps = KTX_FALSE;

    result = ktxTexture2_Create(&createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &texture);
    if (result != KTX_SUCCESS)
    {
        spdlog::error(ktxErrorString(result));
        return;
    }

    size_t offset = 0;
    Gfx::ImageDescription mipDesc{};
    mipDesc.format = format;
    mipDesc.layers = 1;
    mipDesc.mipLevels = 1;
    mipDesc.isCubemap = false;

    uint32_t lw = width;
    uint32_t lh = height;
    uint32_t ld = depth;
    for (int level = 0; level < createInfo.numLevels; ++level)
    {
        mipDesc.width = lw;
        mipDesc.height = lh;
        mipDesc.depth = ld;
        size_t mipSize = mipDesc.GetMipByteSize(0);
        for (int layer = 0; layer < createInfo.numLayers; ++layer)
        {
            for (int face = 0; face < createInfo.numFaces; ++face)
            {
                result = ktxTexture_SetImageFromMemory(ktxTexture(texture), level, layer, face, src + offset, mipSize);
                if (result != KTX_SUCCESS)
                {
                    spdlog::error(ktxErrorString(result));
                    return;
                }

                offset += mipSize;
            }
        }

        lw = lw > 1 ? lw >> 1 : 1;
        lh = lh > 1 ? lh >> 1 : 1;
        ld = ld > 1 ? ld >> 1 : 1;
    }

    if (enableCompression && !texture->isCompressed)
    {
        ktxBasisParams params = {0};
        params.structSize = sizeof(params);
        //// For BasisLZ/ETC1S
        // params.compressionLevel = KTX_ETC1S_DEFAULT_COMPRESSION_LEVEL;
        //// For UASTC
        params.uastc = KTX_TRUE;

        //// Set other BasisLZ/ETC1S or UASTC params to change default quality settings.
        result = ktxTexture2_CompressBasisEx(texture, &params);
        if (result == KTX_SUCCESS)
        {
            char writer[100];
            snprintf(writer, sizeof(writer), "%s version %s", "WeilanEngine", "0");
            ktxHashList_AddKVPair(&texture->kvDataHead, KTX_WRITER_KEY, (ktx_uint32_t)strlen(writer) + 1, writer);
        }
        else
        {
            spdlog::error("failed to compress texture, uncompressed texture is imported instead. {}", ktxErrorString(result));
        }
    }

    ktxTexture_WriteToNamedFile(ktxTexture(texture), path);
    ktxTexture_Destroy(ktxTexture(texture));
}
} // namespace Exporters
