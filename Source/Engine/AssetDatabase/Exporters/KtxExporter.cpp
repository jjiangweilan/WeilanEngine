#include "KtxExporter.hpp"
#include <GfxDriver/Vulkan/Internal/VKEnumMapper.hpp>
#include <glm/glm.hpp>
#include <ktx.h>

namespace Engine::Exporters
{
void KtxExporter::Export(
    const char* path,
    uint8_t* src,
    size_t srcSize,
    uint32_t width,
    uint32_t height,
    uint32_t depth,
    uint32_t dimension,
    uint32_t layer,
    uint32_t mipLevel,
    bool isArray,
    bool isCubemap,
    Gfx::ImageFormat format
)
{
    ktxTexture2* texture;
    ktxTextureCreateInfo createInfo;
    KTX_error_code result;

    createInfo.vkFormat = Gfx::MapFormat(format);
    createInfo.baseWidth = width;
    createInfo.baseHeight = height;
    createInfo.baseDepth = depth;
    createInfo.numDimensions = dimension;
    // Note: it is not necessary to provide a full mipmap pyramid.
    createInfo.numLevels = mipLevel;
    if (isCubemap)
    {
        layer = 1;
    }
    createInfo.numLayers = layer;
    createInfo.numFaces = isCubemap ? 6 : 1;
    createInfo.isArray = isArray;
    createInfo.generateMipmaps = KTX_FALSE;

    result = ktxTexture2_Create(&createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &texture);

    size_t sizePerLayer = srcSize / createInfo.numLayers / createInfo.numFaces;
    size_t offset = 0;
    for (int layer = 0; layer < createInfo.numLayers; ++layer)
    {
        for (int face = 0; face < createInfo.numFaces; ++face)
        {
            result = ktxTexture_SetImageFromMemory(ktxTexture(texture), 0, layer, face, src + offset, sizePerLayer);
            offset += sizePerLayer;
        }
    }

    ktxTexture_WriteToNamedFile(ktxTexture(texture), path);
    ktxTexture_Destroy(ktxTexture(texture));
}
} // namespace Engine::Exporters
