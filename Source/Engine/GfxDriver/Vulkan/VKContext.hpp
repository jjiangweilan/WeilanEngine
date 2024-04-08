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
class VKSwapChainImage;
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

struct Surface
{
    VkSurfaceKHR handle;
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    std::vector<VkPresentModeKHR> surfacePresentModes;
    std::vector<VkSurfaceFormatKHR> surfaceFormats;

    void QuerySurfaceProperties(VkPhysicalDevice gpu);
};

struct Swapchain
{
    ~Swapchain();
    VkSwapchainKHR handle = VK_NULL_HANDLE;
    VkSurfaceFormatKHR surfaceFormat;
    VkExtent2D extent;
    VkImageUsageFlags imageUsageFlags;
    VkSurfaceTransformFlagBitsKHR surfaceTransform;
    VkPresentModeKHR presentMode;
    uint32_t numberOfImages;
    std::unique_ptr<VKSwapChainImage> swapchainImage;

    struct InflightData
    {
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkFence cmdFence = VK_NULL_HANDLE;
        VkSemaphore imageAcquireSemaphore;
        VkSemaphore presendSemaphore;
        uint32_t swapchainIndex;
    };
    std::vector<InflightData> inflightData = {};
    uint32_t currentInflightIndex = 0;

    bool CreateOrOverrideSwapChain(Surface& surface, int& swapchainImageCount, uint32_t width = 0, uint32_t height = 0);
    bool GetImagesFromVulkan();
};

class VKContext
{
public:
    static inline VKContext* Instance()
    {
        return context;
    }
    VKDriver* driver;
    VkDevice device;
    VkInstance instance;
    GPU* gpu;
    Swapchain* swapchain;
    Queue* mainQueue;

    VKMemAllocator* allocator;
    VKObjectManager* objManager;
    VKSharedResource* sharedResource;
    VKDescriptorPoolCache* descriptorPoolCache;

private:
    static VKContext* context;
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
