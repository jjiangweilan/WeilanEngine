#include "VKContext.hpp"
#include "VKSwapchainImage.hpp"
#include <spdlog/spdlog.h>
namespace Gfx
{
VKContext* VKContext::context = nullptr;

void Surface::QuerySurfaceProperties(VkPhysicalDevice gpu)
{
    // Get surface capabilities
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, handle, &surfaceCapabilities) != VK_SUCCESS)
    {
        throw std::runtime_error("Could not check presentation surface capabilities!");
    }

    // Get surface present mode
    uint32_t presentModesCount;
    if ((vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, handle, &presentModesCount, nullptr) != VK_SUCCESS) ||
        (presentModesCount == 0))
    {
        throw std::runtime_error("Error occurred during presentation surface present modes enumeration!");
    }

    surfacePresentModes.resize(presentModesCount);
    if (vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, handle, &presentModesCount, &surfacePresentModes[0]) !=
        VK_SUCCESS)
    {
        SPDLOG_ERROR("Error occurred during presentation surface present modes enumeration!");
    }

    // Get surface formats
    uint32_t formatsCount;
    if ((vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, handle, &formatsCount, nullptr) != VK_SUCCESS) ||
        (formatsCount == 0))
    {
        throw std::runtime_error("Error occurred during presentation surface formats enumeration!");
    }

    surfaceFormats.resize(formatsCount);
    if (vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, handle, &formatsCount, &surfaceFormats[0]) != VK_SUCCESS)
    {
        throw std::runtime_error("Error occurred during presentation surface formats enumeration!");
    }
}

bool Swapchain::CreateOrOverrideSwapChain(Surface& surface, int& swapchainImageCount, uint32_t width, uint32_t height)
{
    VKContext* c = VKContext::Instance();
    vkDeviceWaitIdle(c->device);

    // Get Format
    auto& surfaceFormats = surface.surfaceFormats;

    // Check if list contains most widely used R8 G8 B8 A8 format
    // with nonlinear color space
    bool foundSurfaceFormat = false;
    for (const VkSurfaceFormatKHR& surfaceFormat : surfaceFormats)
    {
        if (surfaceFormat.format == VK_FORMAT_B8G8R8A8_UNORM &&
            surfaceFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            this->surfaceFormat = surfaceFormat;
            foundSurfaceFormat = true;
            break;
        }
    }

    if (!foundSurfaceFormat)
        return false;

    // check surface capabilities
    if ((surface.surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT &
         VK_IMAGE_USAGE_TRANSFER_DST_BIT) ||
        std::find(surface.surfacePresentModes.begin(), surface.surfacePresentModes.end(), VK_PRESENT_MODE_FIFO_KHR) ==
            surface.surfacePresentModes.end())
    {
        throw std::runtime_error("unsupported usage");
    }

    extent = surface.surfaceCapabilities.currentExtent;
    if (width != 0 && height != 0)
    {
        extent = {width, height};
    }
    imageUsageFlags = surface.surfaceCapabilities.supportedUsageFlags;
    surfaceTransform = surface.surfaceCapabilities.currentTransform;
    presentMode = VK_PRESENT_MODE_FIFO_KHR;
    numberOfImages = swapchainImageCount;

    VkSwapchainKHR oldSwapChain = handle;

    VkSwapchainCreateInfoKHR swapChainCreateInfo = {
        VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        nullptr,
        0,
        surface.handle,
        numberOfImages,
        surfaceFormat.format,
        surfaceFormat.colorSpace,
        extent,
        1,
        imageUsageFlags,
        VK_SHARING_MODE_EXCLUSIVE,
        0,
        nullptr,
        surfaceTransform,
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        presentMode,
        VK_TRUE,
        oldSwapChain};

    if (extent.width == 0 || extent.height == 0)
    {
        return false;
    }

    if (vkCreateSwapchainKHR(c->device, &swapChainCreateInfo, nullptr, &handle) != VK_SUCCESS)
    {
        std::runtime_error("Cloud not create swap chain!");
    }

    if (oldSwapChain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(c->device, oldSwapChain, nullptr);
    }

    bool r = GetImagesFromVulkan();
    if (!r)
        return false;

    int minImageCount = surface.surfaceCapabilities.minImageCount;
    int maxImageCount = surface.surfaceCapabilities.maxImageCount;
    swapchainImageCount = swapchainImageCount < minImageCount ? minImageCount : swapchainImageCount;
    swapchainImageCount = swapchainImageCount > maxImageCount ? maxImageCount : swapchainImageCount;

    return true;
}

bool Swapchain::GetImagesFromVulkan()
{
    VKContext* c = VKContext::Instance();
    uint32_t imageCount = 0;

    vkGetSwapchainImagesKHR(c->device, handle, &imageCount, VK_NULL_HANDLE);
    std::vector<VkImage> swapChainImagesTemp(imageCount);
    if (vkGetSwapchainImagesKHR(c->device, handle, &imageCount, swapChainImagesTemp.data()) != VK_SUCCESS)
    {
        return false;
    }

    if (swapchainImage == nullptr)
        swapchainImage = std::make_unique<VKSwapChainImage>();

    swapchainImage->Recreate(swapChainImagesTemp, surfaceFormat.format, extent.width, extent.height, imageUsageFlags);
    swapchainImage->SetName("Swapchain Image");

    return true;
}

Swapchain::~Swapchain() {}

} // namespace Gfx
