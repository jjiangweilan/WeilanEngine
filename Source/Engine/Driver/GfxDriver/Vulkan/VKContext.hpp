#pragma once
#include "Engine/Core/Ptr.hpp"
#include "Engine/Driver/GfxDriver/Vulkan/VKInflightCmd.hpp"
#include "Internal/VKMemAllocator.hpp"
#include "Internal/VKObjectManager.hpp"
#include "Internal/VKSwapChain.hpp"
#include "VKDescriptorPool.hpp"
#include "VKSharedResource.hpp"
namespace Gfx
{
class VKSwapChainImage;
class VKDriver;

enum class TimestampLabelType
{
    Begin,
    End
};

struct TimestampQueryLabel
{
    TimestampLabelType type;
    std::string name;
    uint64_t timestamp;
};

struct CmdBufExecutionReport
{
    std::vector<TimestampQueryLabel> timestampQueryLabels{};
};

struct GfxFeaturesSettings
{
    bool enableGPUProfiling = false;
};

struct Queue
{
    VkQueue handle;
    uint32_t queueIndex;
    uint32_t queueFamilyIndex;

    bool supportTimestamp;
    int maxTimestampQueryCount = 0;
};
struct GPU
{
    VkPhysicalDevice handle;

    VkPhysicalDeviceMemoryProperties memProperties;
    VkPhysicalDeviceFeatures physicalDeviceFeatures{};

    VkPhysicalDeviceProperties physicalDeviceProperties{};
    VkPhysicalDeviceProperties2 physicalDeviceProperties2{};
    VkPhysicalDeviceAccelerationStructurePropertiesKHR asProps = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR};

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

    void AsWin32WindowInteropTexture(const void* sharedHandle, int2 size);
    bool CreateOrOverrideSwapChain(Surface& surface, int& swapchainImageCount, uint32_t width = 0, uint32_t height = 0);

private:
    bool GetImagesFromVulkan();
};

class VKContext
{
public:
    static inline VKContext* Instance() { return context; }
    VKDriver* driver;
    VkCommandPool mainCmdPool; // reference to VKDriver's mainCmdPool
    VkDevice device;
    VkInstance instance;
    GPU* gpu;
    Swapchain* swapchain;
    Queue* mainQueue;
    VKFrameContext* currentFrameContext = nullptr;

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
