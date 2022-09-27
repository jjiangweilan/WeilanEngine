#pragma once
#include "../VKSwapChainImage.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>
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
            VKSwapChain(VKContext* context, VKPhysicalDevice& gpu, VKSurface& surface);
            ~VKSwapChain();
            void RecreateSwapChain(VKDevice* device, VKPhysicalDevice* gpu, VKSurface* surface);
            void AcquireNextImage(uint32_t& imageIndex,  VKImage*& nextPresentImage, VkSemaphore semaphoreToSignal);
            VkResult PresentImage(VkQueue queue, uint32_t presentImageIndex, uint32_t waitSemaphoreCount, VkSemaphore* waitSemaphores);
            std::vector<VKSwapChainImage>& GetSwapChainImages() { return swapChainImages; };
            const VkSwapchainKHR& GetHandle() const {return swapChain; };
            const SwapChainInfo& GetSwapChainInfo() const { return swapChainInfo; };

        private:
            VKContext* context;
            VkSwapchainKHR swapChain;
            VKDevice& attachedDevice;

            std::vector<VKSwapChainImage> swapChainImages;

            bool GetSwapChainImagesFromVulkan();

            void CreateOrOverrideSwapChain(VKDevice* device, VKPhysicalDevice* gpu, VKSurface* surface);

            VkSurfaceFormatKHR GetFormat(VKSurface* surface);
            VkImageUsageFlags GetUsageFlags(VKSurface* surface);
            VkPresentModeKHR GetPresentMode(VKSurface* surface);
    };
}
