#pragma once
#include "Internal/VKDevice.hpp"
#include "Internal/VKMemAllocator.hpp"
#include "Internal/VKObjectManager.hpp"
#include "Internal/VKSwapChain.hpp"
#include "Libs/Ptr.hpp"
#include "VKDescriptorPool.hpp"
#include "VKSharedResource.hpp"
namespace Gfx
{
class VKDriver;

struct Queue
{
    VkQueue handle;
    uint32_t queueIndex;
    uint32_t queueFamilyIndex;
};
struct GPU
{
    VkPhysicalDevice handle;

    VkPhysicalDeviceMemoryProperties memProperties;
    VkPhysicalDeviceProperties physicalDeviceProperties{};
    VkPhysicalDeviceFeatures physicalDeviceFeatures{};

    std::vector<VkQueueFamilyProperties> queueFamilyProperties;
    std::vector<VkExtensionProperties> availableExtensions;
};

struct Swapchain
{
    VkSwapchainKHR handle = VK_NULL_HANDLE;
    VkSurfaceFormatKHR surfaceFormat;
    VkExtent2D extent;
    VkImageUsageFlags imageUsageFlags;
    VkSurfaceTransformFlagBitsKHR surfaceTransform;
    VkPresentModeKHR presentMode;
    uint32_t numberOfImages;
};

class VKContext
{
public:
    static inline RefPtr<VKContext> Instance()
    {
        return context;
    }
    VKDriver* driver;
    VkDevice device;
    GPU* gpu;
    Swapchain* swapchain;
    Queue* mainQueue;

    VKMemAllocator* allocator;
    VKObjectManager* objManager;
    VKSharedResource* sharedResource;
    VKDescriptorPoolCache* descriptorPoolCache;

private:
    static RefPtr<VKContext> context;
    friend class VKDriver;
};

inline VKDriver* GetDriver()
{
    return VKContext::Instance()->driver;
}

inline VkDevice GetDevice()
{
    return VKContext::Instance()->device;
}
inline RefPtr<VKMemAllocator> GetMemAllocator()
{
    return VKContext::Instance()->allocator;
}
inline RefPtr<VKObjectManager> GetObjManager()
{
    return VKContext::Instance()->objManager;
}

inline GPU* GetGPU()
{
    return VKContext::Instance()->gpu;
}

inline Swapchain* GetSwapchain()
{
    return VKContext::Instance()->swapchain;
}

inline Queue* GetMainQueue()
{
    return VKContext::Instance()->mainQueue;
}
} // namespace Gfx
