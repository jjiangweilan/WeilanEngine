#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <SDL2/SDL.h>
#include <glm/glm.hpp>
#include <unordered_map>
#include <vma/vk_mem_alloc.h>
#include <memory>

#include "../GfxDriver.hpp"

#include "VKCommandBuffer.hpp"
#include "VKCommonDefinations.hpp"

#include "VKShaderProgram.hpp"
#include "VKRenderTarget.hpp"
#include "VKRenderContext.hpp"
#include "VKContext.hpp"

namespace Engine::Gfx
{
    class VKAppWindow;
    class VKInstance;
    class VKSurface;
    class VKDevice;
    class VKPhysicalDevice;
    class VKMemAllocator;
    class VKObjectManager;
    class VKFrameBuffer;
    class VKRenderPass;
    class VKSharedResource;
    class VKDriver : public Gfx::GfxDriver
    {
        public:
            VKDriver();
            ~VKDriver() override;

            // Gfx api
            void InitGfxFactory() override;
            std::unique_ptr<RenderTarget> CreateRenderTarget(const RenderTargetDescription& renderTargetDescription) override;
            RenderContext* GetRenderContext() override;
            void SetPresentImage(RefPtr<Image> presentImage) override;
            Extent2D GetWindowSize() override;
            GfxBackend GetGfxBackendType() override;

            void ForceSyncResources() override;
            void DispatchGPUWork() override;

            SDL_Window* GetSDLWindow() override;
        private:
            VKInstance* instance;
            VKDevice* device;
            VKAppWindow* appWindow;
            VKSurface* surface;
            VKPhysicalDevice* gpu;
            VKSwapChain* swapchain;

            VKMemAllocator* memAllocator;
            VKRenderContext* renderContext;
            VKObjectManager* objectManager;

            VkDevice device_vk;
            UniPtr<VKContext> context;
            UniPtr<VKSharedResource> sharedResource;
            // Vulkan objects

            // TODO: first we can extend this to multi command buffer, then we will do multithreading
            VkCommandPool commandPool;
            VkQueue mainQueue;
            VkCommandBuffer mainCommandBuffer;

            std::function<void(VkCommandBuffer)> presentImageFunc;
            VKRenderTarget* finalRenderTarget;
            struct InFlightFrame
            {
                uint32_t imageIndex = -1;
                VKImage* presentImage = nullptr;
                VkSemaphore imageAcquireSemaphore = VK_NULL_HANDLE;
                VkSemaphore renderingFinishedSemaphore = VK_NULL_HANDLE;
                VkFence mainQueueFinishedFence = VK_NULL_HANDLE;
            } inFlightFrame;

            std::shared_ptr<VKCommandBuffer> CreateCommandBuffer(VKCommandUsage usage);
    };
}
