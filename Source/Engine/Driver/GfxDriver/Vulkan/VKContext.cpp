#include "VKContext.hpp"
#include "VKCommon.hpp"

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
        if (surfaceFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
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
    imageUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
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
        oldSwapChain
    };

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

uint32_t FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

void Swapchain::AsWin32WindowInteropTexture(const void* sharedHandle, int2 size)
{
#if WIN32
    // Create the interop image
    auto context = VKContext::Instance();

    if (handle != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(context->device, handle, nullptr);
        handle = VK_NULL_HANDLE;
    }

    VkExternalMemoryImageCreateInfo extImageInfo = {};
    extImageInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    extImageInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT;

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.pNext = &extImageInfo;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
    imageInfo.extent = {static_cast<uint32_t>(size.x), static_cast<uint32_t>(size.y), 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.queueFamilyIndexCount = 1;
    imageInfo.pQueueFamilyIndices = &VKContext::Instance()->mainQueue->queueFamilyIndex;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage vkImage;
    if (vkCreateImage(context->device, &imageInfo, nullptr, &vkImage) != VK_SUCCESS)
    {
        SPDLOG_ERROR("Failed to create interop image");
        return;
    }

    VkMemoryWin32HandlePropertiesKHR handleProperties = {};
    handleProperties.sType = VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR;

    VkMemoryDedicatedAllocateInfo dedicatedAllocInfo = {};
    dedicatedAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
    dedicatedAllocInfo.image = vkImage;

    // Bind image memory
    HANDLE externalTextureHandle = (HANDLE)sharedHandle;
    VkImportMemoryWin32HandleInfoKHR handleInfo = {};
    handleInfo.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR;
    handleInfo.pNext = &dedicatedAllocInfo;
    handleInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT;
    handleInfo.handle = externalTextureHandle;

    if (vkGetMemoryWin32HandlePropertiesKHR(context->device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT, externalTextureHandle, &handleProperties) != VK_SUCCESS)
    {
        SPDLOG_ERROR("Failed to get Win32 handle properties");
        return;
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(context->device, vkImage, &memRequirements);

    uint32_t memoryTypeBits = memRequirements.memoryTypeBits & handleProperties.memoryTypeBits;
    if (memoryTypeBits == 0)
    {
        SPDLOG_ERROR("No compatible memory type found for interop image and handle");
        return;
    }

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.pNext = &handleInfo;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(context->gpu->handle, memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkDeviceMemory vkMemory;
    VkResult result = vkAllocateMemory(context->device, &allocInfo, nullptr, &vkMemory);
    if (result != VK_SUCCESS)
    {
        SPDLOG_ERROR("Failed to allocate interop memory, VkResult: {}", (int)result);
        return;
    }
    if (vkBindImageMemory(context->device, vkImage, vkMemory, 0) != VK_SUCCESS)
    {
        SPDLOG_ERROR("Failed to bind interop image memory");
        return;
    }

    VkImage vkImages[1] = {vkImage};
    swapchainImage->Recreate(vkImages, imageInfo.format, size.x, size.y, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    swapchainImage->SetActiveSwapChainImage(0);
    swapchainImage->SetName("Win32 Interop Texture");
#endif
}

Swapchain::~Swapchain() {}

} // namespace Gfx
