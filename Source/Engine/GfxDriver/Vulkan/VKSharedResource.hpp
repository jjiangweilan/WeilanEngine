#pragma once

#include "Libs/Ptr.hpp"
#include <vulkan/vulkan.h>
namespace Engine::Gfx
{
class VKContext;
class VKImage;
// class VKStorageBuffer;
class VKSharedResource
{
public:
    VKSharedResource(RefPtr<VKContext> context);
    ~VKSharedResource();
    VkSampler GetDefaultSampler()
    {
        return defaultSampler;
    }
    RefPtr<VKImage> GetDefaultTexture2D()
    {
        return defaultTexture;
    }
    VKImage* GetDefaultTextureCube()
    {
        return defaultCubemap.get();
    }
    // RefPtr<VKStorageBuffer> GetDefaultStorageBuffer() {return defaultStorageBuffer; }

private:
    RefPtr<VKContext> context;

    // UniPtr<VKStorageBuffer> defaultStorageBuffer;
    VkSampler defaultSampler;
    UniPtr<VKImage> defaultTexture;
    std::unique_ptr<VKImage> defaultCubemap;
};
} // namespace Engine::Gfx
