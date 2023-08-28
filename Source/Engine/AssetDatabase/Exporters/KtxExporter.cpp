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
    ktx_uint32_t level, faceSlice;

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

    level = createInfo.numLevels;
    layer = createInfo.numLayers;
    faceSlice = createInfo.numFaces;
    result = ktxTexture_SetImageFromMemory(ktxTexture(texture), level, layer, faceSlice, src, srcSize);

    ktxTexture_WriteToNamedFile(ktxTexture(texture), path);
    ktxTexture_Destroy(ktxTexture(texture));
}
} // namespace Engine::Exporters
