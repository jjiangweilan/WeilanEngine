#pragma once
#include "../VKImage.hpp"
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
    RefPtr<Image> GetSwapChainImage(int index) { return &swapChainImages[index]; }
    void RecreateSwapChain(VKDevice* device, VKPhysicalDevice* gpu, VKSurface* surface);
    bool AcquireNextImage(RefPtr<VKSwapChainImageProxy> swapChainImageProxy, VkSemaphore semaphoreToSignal);
    VkSwapchainKHR GetHandle() const { return swapChain; };
    const SwapChainInfo& GetSwapChainInfo() const { return swapChainInfo; };

private:
    class VKSwapChainImage : public VKImage
    {
    public:
        VKSwapChainImage(VkImage image, VkFormat format, uint32_t width, uint32_t height);
        VKSwapChainImage(VKSwapChainImage&& other) : VKImage(std::move(other)){};
        VKSwapChainImage() = default;
        ~VKSwapChainImage() override;
    };
    VkSwapchainKHR swapChain;
    RefPtr<VKDevice> attachedDevice;
    std::vector<VKSwapChainImage> swapChainImages;
    uint32_t graphicsQueueFamilyIndex;
    VKSurface* surface;

    bool GetSwapChainImagesFromVulkan();

    void CreateOrOverrideSwapChain(VKDevice* device, VKPhysicalDevice* gpu, VKSurface* surface);

    VkSurfaceFormatKHR GetFormat(VKSurface* surface);
    VkImageUsageFlags GetUsageFlags(VKSurface* surface);
    VkPresentModeKHR GetPresentMode(VKSurface* surface);
};
} // namespace Engine::Gfx
