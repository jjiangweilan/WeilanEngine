#pragma once
#include "Code/Ptr.hpp"
#include "Internal/VKDevice.hpp"
#include "Internal/VKMemAllocator.hpp"
#include "Internal/VKObjectManager.hpp"
#include "Internal/VKSwapChain.hpp"
#include "VKDescriptorPool.hpp"
#include "VKSharedResource.hpp"
#include "VKStructs.hpp"
namespace Engine::Gfx
{
    class VKDevice;
    class VKMemAllocator;
    class VKObjectManager;
    class VKSharedResource;
    class VKSwapChain;
    class VKDriver;
    class VKContext
    {
        public:
            static inline RefPtr<VKContext> Instance() { return context; }
            RefPtr<VKDevice> device;
            RefPtr<VKMemAllocator> allocator;
            RefPtr<VKObjectManager> objManager;
            RefPtr<VKSharedResource> sharedResource;
            RefPtr<VKSwapChain> swapchain;
            RefPtr<const DeviceQueue> mainQueue;
            RefPtr<VKDescriptorPoolCache> descriptorPoolCache;
        private:
            static RefPtr<VKContext> context;
            friend class VKDriver;
    };
}
