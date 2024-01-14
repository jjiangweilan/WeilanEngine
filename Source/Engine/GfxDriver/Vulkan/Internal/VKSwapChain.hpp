//#pragma once
//#include "../VKSwapchainImage.hpp"
//#include "VKDevice.hpp"
//#include <unordered_map>
//#include <vector>
//#include <vulkan/vulkan.h>
//namespace Gfx
//{
//
//class VKDevice;
//class VKPhysicalDevice;
//class VKSurface;
//enum class AcquireNextImageResult
//{
//    Succeeded,
//    Recreated,
//    Failed
//};
//
//class VKSwapChain
//{
//private:
//    struct SwapChainInfo
//    {
//        VkSurfaceFormatKHR surfaceFormat;
//        VkExtent2D extent;
//        VkImageUsageFlags imageUsageFlags;
//        VkSurfaceTransformFlagBitsKHR surfaceTransform;
//        VkPresentModeKHR presentMode;
//        uint32_t numberOfImages;
//    } swapChainInfo;
//
//public:
//    VKSwapChain(
//        uint32_t graphicsQueueFamilyIndex, int swapchainCount, RefPtr<VKPhysicalDevice> gpu, VKSurface& surface
//    );
//    ~VKSwapChain();
//
//    bool RecreateSwapChain(VKDevice* device, VKPhysicalDevice* gpu, VKSurface* surface);
//    AcquireNextImageResult AcquireNextImage(VkSemaphore semaphoreToSignal, uint32_t& nextPresentImageIndex);
//    VKSwapChainImage* GetSwapchainImage()
//    {
//        return swapChainImage.get();
//    }
//    VkSwapchainKHR GetHandle() const
//    {
//        return swapChain;
//    }
//    const SwapChainInfo& GetSwapChainInfo() const
//    {
//        return swapChainInfo;
//    }
//
//private:
//    RefPtr<VKDevice> attachedDevice;
//    VkSwapchainKHR swapChain;
//    VKSurface* surface;
//    std::unique_ptr<VKSwapChainImage> swapChainImage;
//    uint32_t graphicsQueueFamilyIndex;
//    int swapchainCount;
//
//    bool GetSwapChainImagesFromVulkan();
//    bool CreateOrOverrideSwapChain(VKDevice* device, VKPhysicalDevice* gpu, VKSurface* surface);
//
//    VkSurfaceFormatKHR GetFormat(VKSurface* surface);
//    VkImageUsageFlags GetUsageFlags(VKSurface* surface);
//    VkPresentModeKHR GetPresentMode(VKSurface* surface);
//};
//} // namespace Gfx
