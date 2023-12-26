#include "VKSharedResource.hpp"
#include "Internal/VKObjectManager.hpp"
#include "VKContext.hpp"
#include "VKDriver.hpp"
#include "VKImage.hpp"
namespace Gfx
{
VKSharedResource::VKSharedResource(VKDriver* driver) : driver(driver)
{
    // create default sampler
    VkSamplerCreateInfo samplerCreateInfo{};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.pNext = VK_NULL_HANDLE;
    samplerCreateInfo.flags = 0;
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR; // VK_FILTER_NEAREST;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR; // VK_FILTER_NEAREST;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.mipLodBias = 0;
    samplerCreateInfo.anisotropyEnable = VK_FALSE;
    samplerCreateInfo.maxAnisotropy = 0;
    samplerCreateInfo.compareEnable = VK_FALSE;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    samplerCreateInfo.minLod = 0;
    samplerCreateInfo.maxLod = VK_LOD_CLAMP_NONE;
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
    driver->objectManager->CreateSampler(samplerCreateInfo, defaultSampler);

    // create default image
    ImageDescription desc{};
    desc.format = ImageFormat::R8G8B8A8_UNorm;
    desc.height = 2;
    desc.width = 2;
    desc.mipLevels = 1;
    desc.isCubemap = false;
    desc.multiSampling = MultiSampling::Sample_Count_1;
    defaultTexture = MakeUnique<VKImage>(desc, ImageUsage::Texture | ImageUsage::TransferDst);
    defaultTexture->SetName("Default Texture");

    ImageDescription descCube{};
    descCube.format = ImageFormat::R8G8B8A8_UNorm;
    descCube.height = 2;
    descCube.width = 2;
    descCube.isCubemap = true;
    descCube.multiSampling = MultiSampling::Sample_Count_1;
    descCube.mipLevels = 1;
    // uint8_t pxls[16] = {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};
    // desc.data = (unsigned char*)pxls;
    descCube.multiSampling = MultiSampling::Sample_Count_1;
    defaultCubemap = MakeUnique<VKImage>(descCube, ImageUsage::Texture | ImageUsage::TransferDst);
    defaultCubemap->SetName("Default Cubemap");
}

VKSharedResource::~VKSharedResource()
{
    driver->objectManager->DestroySampler(defaultSampler);
}
} // namespace Gfx
