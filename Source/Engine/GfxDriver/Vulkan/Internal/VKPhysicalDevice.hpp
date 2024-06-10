#pragma once

#include <vector>
#include <vulkan/vulkan.h>
namespace Gfx
{
class VKInstance;
class VKSurface;
class VKPhysicalDevice
{
public:
    const VkPhysicalDeviceProperties& GetPhysicalDeviceProperties() const
    {
        return physicalDeviceProperties;
    }

    const VkPhysicalDeviceFeatures& GetPhysicalDeviceFeatures() const
    {
        return physicalDeviceFeatures;
    }

    const std::vector<VkQueueFamilyProperties>& GetQueueFamilyProperties() const
    {
        return queueFamilyProperties;
    }

    const std::vector<VkExtensionProperties>& GetAvailableExtensions() const
    {
        return availableExtensions;
    }

    // const uint32_t& GetTransferQueueFamilyIndex() const { return graphicsQueueFamilyIndex; }

    VkPhysicalDevice GetHandle() const
    {
        return gpu;
    }

    VkFormat PickSupportedFormat(
        const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags featureFlags
    );

    static VKPhysicalDevice SelectGPUAndQueryDataForSurface(VKInstance& instance, VKSurface& surface);

    VKPhysicalDevice(VkPhysicalDevice gpu, VKSurface* surface);

    uint32_t FindMemoryTypeIndex(VkMemoryRequirements& memRequirement, VkMemoryPropertyFlags memPropertiesFlag);

    uint32_t GetFormatSize(VkFormat format);

private:
    VkPhysicalDevice gpu = VK_NULL_HANDLE;

    VkPhysicalDeviceMemoryProperties memProperties;
    VkPhysicalDeviceProperties physicalDeviceProperties{};
    VkPhysicalDeviceFeatures physicalDeviceFeatures{};

    std::vector<VkQueueFamilyProperties> queueFamilyProperties;
    std::vector<VkExtensionProperties> availableExtensions;

    static std::vector<VKPhysicalDevice> GetAllPhysicalDevices(VKInstance* instance, VKSurface* surface);
    void GetRequiredQueuesFamilyIndices(VKPhysicalDevice& physicalDevice);
    void QuerySurfaceData(VKInstance* attachedInstance);

    friend class GfxContext;
};
} // namespace Gfx
