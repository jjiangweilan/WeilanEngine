#pragma once
#include "../VKCommandQueue.hpp"
#include "VKPhysicalDevice.hpp"
#include <vector>
#include <vulkan/vulkan.h>

namespace Gfx
{
class VKInstance;
class VKSurface;

class VKDevice
{
public:
    struct QueueRequest
    {
        VkQueueFlags flags;
        bool requireSurfaceSupport;
        float priority;
    };

    VKDevice(VKInstance* instance, VKSurface* surface, QueueRequest* requests, int requestsCount);
    ~VKDevice();

    VKPhysicalDevice& GetGPU()
    {
        return gpu;
    }

    void WaitForDeviceIdle();

    VkDevice GetHandle() const
    {
        return deviceHandle;
    }
    VKCommandQueue& GetQueue(int i)
    {
        return queues[i];
    }

    uint32_t GetBufferingCount() const
    {
        return BUFFERING_COUNT;
    }

    void ResetTransferBuffer();
    void TransferBuffer();

private:
    const uint32_t BUFFERING_COUNT = 2;
    VkDevice deviceHandle;
    VKPhysicalDevice gpu;

    VkPhysicalDeviceFeatures requiredDeviceFeatures{}; // no feature required yet
    std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME};

    std::vector<VKCommandQueue> queues;

    friend class GfxContext;
};
} // namespace Gfx
