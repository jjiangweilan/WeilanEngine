#pragma once
#include "Code/Ptr.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <functional>
namespace Engine::Gfx
{
    class VKDevice;
    class VKMemAllocator;
    class VKObjectManager;
    class VKSharedResource;
    class VKSwapChain;
    struct VKContext
    {
        RefPtr<VKDevice> device;
        RefPtr<VKMemAllocator> allocator;
        RefPtr<VKObjectManager> objManager;
        RefPtr<VKSharedResource> sharedResource;
        RefPtr<VKSwapChain> swapchain;

        void AppendFramePrepareCommands(std::function<void(VkCommandBuffer)>&& cmdBuf);
        void RecordFramePrepareCommands(VkCommandBuffer cmdBuf);

        private:
        std::vector<std::function<void(VkCommandBuffer)>> pendingPrepareCommands;
    };
}
