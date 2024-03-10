#pragma once

#include "Libs/Ptr.hpp"
#include <vulkan/vulkan.h>
namespace Gfx
{
class VKDriver;
class VKImage;
// class VKStorageBuffer;
class VKSharedResource
{
public:
    VKSharedResource(VKDriver* driver);
    ~VKSharedResource();
    VkSampler GetDefaultSampler()
    {
        return defaultSampler;
    }
    RefPtr<VKImage> GetDefaultTexture2D()
    {
        return defaultTexture;
    }
    VKImage* GetDefaultTexture3D()
    {
        return defaultTexture3D.get();
    }
    VKImage* GetDefaultStoargeImage2D()
    {
        return defaultStorage2DImage.get();
    }
    VKImage* GetDefaultTextureCube()
    {
        return defaultCubemap.get();
    }
    // RefPtr<VKStorageBuffer> GetDefaultStorageBuffer() {return defaultStorageBuffer; }

private:
    VKDriver* driver;
    // UniPtr<VKStorageBuffer> defaultStorageBuffer;
    VkSampler defaultSampler;
    UniPtr<VKImage> defaultTexture;
    std::unique_ptr<VKImage> defaultTexture3D;
    std::unique_ptr<VKImage> defaultStorage2DImage;
    std::unique_ptr<VKImage> defaultCubemap;
};
} // namespace Gfx
