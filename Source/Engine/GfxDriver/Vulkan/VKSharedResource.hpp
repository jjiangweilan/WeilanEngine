#pragma once

#include "Code/Ptr.hpp"
#include <vulkan/vulkan.h>

namespace Engine::Gfx
{
    class VKContext;
    class VKImage;
    class VKStorageBuffer;
    class VKSharedResource
    {
        public:
            VKSharedResource(RefPtr<VKContext> context);
            ~VKSharedResource();
            VkSampler GetDefaultSampler() {return defaultSampler; }
            RefPtr<VKImage> GetDefaultTexture() {return defaultTexture; }
            RefPtr<VKStorageBuffer> GetDefaultStorageBuffer() {return defaultStorageBuffer; }


        private:
            RefPtr<VKContext> context;

            UniPtr<VKStorageBuffer> defaultStorageBuffer;
            VkSampler defaultSampler;
            UniPtr<VKImage> defaultTexture;
    };
}
