#include "VKPhysicalDevice.hpp"
#include "VKInstance.hpp"
#include "VKSurface.hpp"

#include <set>
#include <stdexcept>
#include <string>
#include <vector>
namespace Gfx
{
VKPhysicalDevice::VKPhysicalDevice(VkPhysicalDevice gpu, VKSurface* surface)
{
    this->gpu = gpu;
    vkGetPhysicalDeviceProperties(gpu, &physicalDeviceProperties);
    vkGetPhysicalDeviceFeatures(gpu, &physicalDeviceFeatures);
    vkGetPhysicalDeviceMemoryProperties(gpu, &memProperties);

    uint32_t count;
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, VK_NULL_HANDLE);
    queueFamilyProperties.resize(count);
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, queueFamilyProperties.data());

    vkEnumerateDeviceExtensionProperties(gpu, nullptr, &count, nullptr);

    availableExtensions.resize(count);
    vkEnumerateDeviceExtensionProperties(gpu, nullptr, &count, availableExtensions.data());
}

std::vector<VKPhysicalDevice> VKPhysicalDevice::GetAllPhysicalDevices(VKInstance* instance, VKSurface* surface)
{
    // Get all physical devices
    uint32_t count;
    std::vector<VkPhysicalDevice> vulkanPhysicalDevices;

    VkResult result = vkEnumeratePhysicalDevices(instance->GetHandle(), &count, nullptr);
    if (result != VK_SUCCESS)
    {
        std::runtime_error("no physical devices enumerated");
    }

    vulkanPhysicalDevices.resize(count);

    result = vkEnumeratePhysicalDevices(instance->GetHandle(), &count, vulkanPhysicalDevices.data());
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("can't get physical devices");
    }

    std::vector<VKPhysicalDevice> physicalDevices;
    for (auto& vulkanPhysicalDevice : vulkanPhysicalDevices)
    {
        physicalDevices.emplace_back(vulkanPhysicalDevice, surface);
    }

    if (physicalDevices.size() == 0)
        throw std::runtime_error("No physical device found!");

    return physicalDevices;
}

VKPhysicalDevice VKPhysicalDevice::SelectGPUAndQueryDataForSurface(VKInstance& instance, VKSurface& surface)
{
    for (auto& physicalDevice : GetAllPhysicalDevices(&instance, &surface))
    {
        // Check required device extensions
        std::set<std::string> requiredExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

        for (const auto& extension : physicalDevice.availableExtensions)
        {
            requiredExtensions.erase(extension.extensionName);
        }

        if (!requiredExtensions.empty())
            continue; // early skip if this device is not suitable

        // Check required device features
        if (!true) // no required feature yet
            continue;

        // This physicalDevices passed all the test!
        return physicalDevice;
        break;
    }

    throw std::runtime_error("No Suitable GPU");
}

VkFormat VKPhysicalDevice::PickSupportedFormat(
    const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags featureFlags
)
{
    for (VkFormat format : candidates)
    {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(gpu, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & featureFlags) != 0)
        {
            return format;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & featureFlags) != 0)
        {
            return format;
        }
    }

    throw std::runtime_error("failed to find supported format!");
}

uint32_t VKPhysicalDevice::FindMemoryTypeIndex(
    VkMemoryRequirements& memRequirement, VkMemoryPropertyFlags memPropertiesFlag
)
{
    uint32_t memoryTypeBits = memRequirement.memoryTypeBits;

    for (int i = 0; i < memProperties.memoryTypeCount; ++i)
    {
        if ((1 << i & memoryTypeBits) && (memProperties.memoryTypes[i].propertyFlags & memPropertiesFlag))
        {
            return i;
        }
    }

    throw std::runtime_error("Can't find fitted memory type");
}
} // namespace Gfx
