#include "VKSwapChain.hpp"
#include "VKDevice.hpp"
#include "VKSurface.hpp"
#include "../VKContext.hpp"
#include <iostream>
#include <spdlog/spdlog.h>
namespace Engine::Gfx
{
    VKSwapChain::VKSwapChain(VKContext* context, VKPhysicalDevice& gpu, VKSurface& surface) : context(context), attachedDevice(*context->device), swapChain(VK_NULL_HANDLE)
    {
        CreateOrOverrideSwapChain(context->device.Get(), &gpu, &surface);
    }

    VKSwapChain::~VKSwapChain()
    {
        vkDestroySwapchainKHR(attachedDevice.GetHandle(), swapChain, VK_NULL_HANDLE);
    }

    void VKSwapChain::CreateOrOverrideSwapChain(VKDevice* device, VKPhysicalDevice* gpu, VKSurface* surface)
    {

        swapChainInfo.surfaceFormat = GetFormat(surface);
        swapChainInfo.extent = surface->GetSurfaceCapabilities().currentExtent;
        swapChainInfo.imageUsageFlags = GetUsageFlags(surface);
        swapChainInfo.surfaceTransform = surface->GetSurfaceCapabilities().currentTransform;
        swapChainInfo.presentMode = GetPresentMode(surface);
        VkSwapchainKHR oldSwapChain = swapChain;

        // What image count we use depends on what presentation mode we choose
        if (swapChainInfo.presentMode == VK_PRESENT_MODE_FIFO_KHR ||
                swapChainInfo.presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            uint32_t minImageCount = surface->GetSurfaceCapabilities().minImageCount;
            swapChainInfo.numberOfImages = minImageCount + 1;
        }
        else
        {
            throw std::runtime_error("Unhandled swapchin present mode");
        }

        if (swapChainInfo.numberOfImages > surface->GetSurfaceCapabilities().maxImageCount)
            throw std::runtime_error("swap chain can't support requested amount of imgaes");

        if (static_cast<int>(swapChainInfo.imageUsageFlags) == -1 || static_cast<int>(swapChainInfo.presentMode) == -1)
        {
            throw std::runtime_error("desired usage and desired present mode is not found");
        }

        VkSwapchainCreateInfoKHR swapChainCreateInfo = {
            VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            nullptr,
            0,
            surface->GetHandle(),
            swapChainInfo.numberOfImages,
            swapChainInfo.surfaceFormat.format,
            swapChainInfo.surfaceFormat.colorSpace,
            swapChainInfo.extent,
            1,
            swapChainInfo.imageUsageFlags,
            VK_SHARING_MODE_EXCLUSIVE,
            0,
            nullptr,
            swapChainInfo.surfaceTransform,
            VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            swapChainInfo.presentMode,
            VK_TRUE,
            oldSwapChain
        };

        if (vkCreateSwapchainKHR(device->GetHandle(), &swapChainCreateInfo, nullptr, &swapChain) != VK_SUCCESS)
        {
            std::runtime_error("Cloud not create swap chain!");
        }

        if (oldSwapChain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(device->GetHandle(), oldSwapChain, nullptr);
        }

        GetSwapChainImagesFromVulkan();
    }

    void VKSwapChain::RecreateSwapChain(VKDevice* device, VKPhysicalDevice* gpu, VKSurface* surface)
    {
        CreateOrOverrideSwapChain(device, gpu, surface);
    }

    VkSurfaceFormatKHR VKSwapChain::GetFormat(VKSurface* gpu)
    {
        auto &surfaceFormats = gpu->GetSurfaceFormats();

        // Check if list contains most widely used R8 G8 B8 A8 format
        // with nonlinear color space
        for (const VkSurfaceFormatKHR &surface_format : surfaceFormats)
        {
            if (surface_format.format == VK_FORMAT_B8G8R8A8_UNORM && surface_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                return surface_format;
            }
        }

        SPDLOG_WARN("No requested swapchain format found, use the first one provided");
        return surfaceFormats[0];
    }

    bool VKSwapChain::GetSwapChainImagesFromVulkan()
    {
        swapChainImages.clear();
        uint32_t imageCount = 0;

        vkGetSwapchainImagesKHR(attachedDevice.GetHandle(), swapChain, &imageCount, VK_NULL_HANDLE);

        std::vector<VkImage> swapChainImagesTemp(imageCount);

        if (vkGetSwapchainImagesKHR(attachedDevice.GetHandle(), swapChain, &imageCount, swapChainImagesTemp.data()) != VK_SUCCESS)
        {
            return false;
        }

        for(uint32_t i = 0; i < imageCount; ++i)
        {
            swapChainImages.emplace_back(context, swapChainImagesTemp[i], swapChainInfo.surfaceFormat.format, swapChainInfo.extent.width, swapChainInfo.extent.height);
        }
        
        return true;
    }

    VkImageUsageFlags VKSwapChain::GetUsageFlags(VKSurface* surface)
    {
        // Color attachment flag must always be supported
        // We can define other usage flags but we always need to check if they are supported
        const auto &surfaceCapabilities = surface->GetSurfaceCapabilities();
        if (surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        {
            return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        }
        std::cout << "VK_IMAGE_USAGE_TRANSFER_DST image usage is not supported by the swap chain!" << std::endl
            << "Supported swap chain's image usages include:" << std::endl
            << (surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT ? "    VK_IMAGE_USAGE_TRANSFER_SRC\n" : "")
            << (surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT ? "    VK_IMAGE_USAGE_TRANSFER_DST\n" : "")
            << (surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_SAMPLED_BIT ? "    VK_IMAGE_USAGE_SAMPLED\n" : "")
            << (surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_STORAGE_BIT ? "    VK_IMAGE_USAGE_STORAGE\n" : "")
            << (surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT ? "    VK_IMAGE_USAGE_COLOR_ATTACHMENT\n" : "")
            << (surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT ? "    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT\n" : "")
            << (surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT ? "    VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT\n" : "")
            << (surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT ? "    VK_IMAGE_USAGE_INPUT_ATTACHMENT" : "")
            << std::endl;
        return static_cast<VkImageUsageFlags>(-1);
    }

    VkPresentModeKHR VKSwapChain::GetPresentMode(VKSurface* surface)
    {
        for (const VkPresentModeKHR &presentMode : surface->GetSurfacePresentModes())
        {
            if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                return presentMode;
            }
        }

        for (const VkPresentModeKHR &presentMode : surface->GetSurfacePresentModes())
        {
            if (presentMode == VK_PRESENT_MODE_FIFO_KHR)
            {
                return presentMode;
            }
        }

        return static_cast<VkPresentModeKHR>(-1);
    }

    void VKSwapChain::AcquireNextImage(uint32_t& nextPresentImageIndex, VKImage*& nextPresentImage, VkSemaphore semaphoreToSignal)
    {
        VkResult result = vkAcquireNextImageKHR(attachedDevice.GetHandle(), swapChain, -1, semaphoreToSignal, VK_NULL_HANDLE, &nextPresentImageIndex);
        nextPresentImage = static_cast<VKImage*>(&swapChainImages[nextPresentImageIndex]);
        switch (result)
        {
            case VK_SUCCESS:
                break;
            case VK_SUBOPTIMAL_KHR:
            case VK_ERROR_OUT_OF_DATE_KHR:
                break;
            default:
                throw std::runtime_error("Vulkan error: Problem occurred during swap chain image acquisition!");
        }
    }

    VkResult VKSwapChain::PresentImage(VkQueue queue, uint32_t presentImageIndex, uint32_t waitSemaphoreCount, VkSemaphore* waitSemaphores)
    {
        VkPresentInfoKHR presentInfo = {
            VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            nullptr,
            waitSemaphoreCount,
            waitSemaphores,
            1,
            &swapChain,
            &presentImageIndex,
            nullptr                                 
        };

        VkResult result = vkQueuePresentKHR(queue, &presentInfo);
        return result;
    }
}
