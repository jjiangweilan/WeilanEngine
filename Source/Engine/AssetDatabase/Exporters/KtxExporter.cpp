#include "KtxExporter.hpp"
#include <GfxDriver/Vulkan/Internal/VKEnumMapper.hpp>
#include <glm/glm.hpp>
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
    Gfx::ImageFormat format,
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
    const size_t formatByteSize = Gfx::MapImageFormatToByteSize(format);
    for (int layer = 0; layer < createInfo.numLayers; ++layer)
    {
        for (int face = 0; face < createInfo.numFaces; ++face)
        {
            int lw = width;
            int lh = height;
            for (int level = 0; level < createInfo.numLevels; ++level)
            {
                size_t mipSize = lw * lh * formatByteSize;
                result = ktxTexture_SetImageFromMemory(ktxTexture(texture), level, layer, face, src + offset, mipSize);
                if (result != KTX_SUCCESS)
                {
                    spdlog::error(ktxErrorString(result));
                    return;
                }
                lw *= glm::pow(0.5, level);
                lh *= glm::pow(0.5, level);
                offset += mipSize;
            }
        }
    }

    if (enableCompression)
    {
        // ktxBasisParams params = {0};
        // params.structSize = sizeof(params);
        //// For BasisLZ/ETC1S
        // params.compressionLevel = KTX_ETC1S_DEFAULT_COMPRESSION_LEVEL;
        //// For UASTC
        // params.uastc = KTX_TRUE;
        //// Set other BasisLZ/ETC1S or UASTC params to change default quality settings.
        result = ktxTexture2_CompressBasis(texture, 200);
        if (result != KTX_SUCCESS)
        {
            spdlog::error(ktxErrorString(result));
            return;
        }
        char writer[100];
        snprintf(writer, sizeof(writer), "%s version %s", "WeilanEngine", 0);
        ktxHashList_AddKVPair(&texture->kvDataHead, KTX_WRITER_KEY, (ktx_uint32_t)strlen(writer) + 1, writer);
    }

    ktxTexture_WriteToNamedFile(ktxTexture(texture), path);
    ktxTexture_Destroy(ktxTexture(texture));
}
} // namespace Exporters
