#pragma once
#include "../VKSwapchainImage.hpp"
#include "VKDevice.hpp"
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>
namespace Engine::Gfx
{

class VKDevice;
class VKPhysicalDevice;
class VKSurface;
class VKSwapChain
{
private:
    struct SwapChainInfo
    {
        VkSurfaceFormatKHR surfaceFormat;
        VkExtent2D extent;
        VkImageUsageFlags imageUsageFlags;
        VkSurfaceTransformFlagBitsKHR surfaceTransform;
        VkPresentModeKHR presentMode;
        uint32_t numberOfImages;
    } swapChainInfo;

public:
    VKSwapChain(uint32_t graphicsQueueFamilyIndex, RefPtr<VKPhysicalDevice> gpu, VKSurface& surface);
    ~VKSwapChain();

    void RecreateSwapChain(VKDevice* device, VKPhysicalDevice* gpu, VKSurface* surface);
    bool AcquireNextImage(VkSemaphore semaphoreToSignal);
    VKSwapChainImage* GetSwapchainImage()
    {
        return swapChainImage.get();
    }
    VkSwapchainKHR GetHandle() const
    {
        return swapChain;
    }
    const SwapChainInfo& GetSwapChainInfo() const
    {
        return swapChainInfo;
    }

private:
    VkSwapchainKHR swapChain;
    RefPtr<VKDevice> attachedDevice;
    std::unique_ptr<VKSwapChainImage> swapChainImage;
    uint32_t graphicsQueueFamilyIndex;
    VKSurface* surface;

    bool GetSwapChainImagesFromVulkan();

    void CreateOrOverrideSwapChain(VKDevice* device, VKPhysicalDevice* gpu, VKSurface* surface);

    VkSurfaceFormatKHR GetFormat(VKSurface* surface);
    VkImageUsageFlags GetUsageFlags(VKSurface* surface);
    VkPresentModeKHR GetPresentMode(VKSurface* surface);
};
} // namespace Engine::Gfx
