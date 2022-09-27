#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include "VKPhysicalDevice.hpp"

namespace Engine::Gfx
{
    class VKInstance;
    class VKSurface;
    class VKDevice
    {
        public:
            VKDevice(VKInstance* instance, VKSurface* surface);
            ~VKDevice();

            VKPhysicalDevice& GetGPU() { return gpu; }

            void WaitForDeviceIdle();

            VkDevice GetHandle() const { return deviceHandle; }
            VkQueue GetGraphicsQueue() const {return graphicsQueue;}
            VkQueue GetTransferQueue() const {return transferQueue;}
            VkQueue GetComputeQueue() const {return computeQueue;}

            uint32_t GetBufferingCount() const {return BUFFERING_COUNT;}

            void ResetTransferBuffer();
            void TransferBuffer();
        private:

            const uint32_t BUFFERING_COUNT = 2;
            VkDevice deviceHandle;
            VKPhysicalDevice gpu;

            VkPhysicalDeviceFeatures requiredDeviceFeatures{}; // no feature required yet
            std::vector<const char *> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME};

            VkQueue graphicsQueue = VK_NULL_HANDLE;
            VkQueue transferQueue = VK_NULL_HANDLE;
            VkQueue computeQueue = VK_NULL_HANDLE;

            friend class GfxContext;
    };
}
