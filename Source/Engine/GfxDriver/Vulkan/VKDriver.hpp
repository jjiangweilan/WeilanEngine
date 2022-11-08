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
    class VKSwapChainImageProxy;
    class VKContext;
    class VKDescriptorPoolCache;
    class VKDriver : public Gfx::GfxDriver
    {
        public:
            VKDriver();
            ~VKDriver() override;

            void ExecuteCommandBuffer(UniPtr<CommandBuffer>&& cmdBuf) override;
            Extent2D GetWindowSize() override;
            Backend GetGfxBackendType() override;
            void ForceSyncResources() override;
            void DispatchGPUWork() override;
            void WaitForIdle() override;
            SDL_Window* GetSDLWindow() override;
            RefPtr<Image> GetSwapChainImageProxy() override;
            UniPtr<CommandBuffer> CreateCommandBuffer() override;
            UniPtr<GfxBuffer> CreateBuffer(uint32_t size, BufferUsage usage, bool cpuVisible = false) override;
            UniPtr<ShaderResource> CreateShaderResource(RefPtr<ShaderProgram> shader, ShaderResourceFrequency frequency) override;
            UniPtr<RenderPass> CreateRenderPass() override;
            UniPtr<FrameBuffer> CreateFrameBuffer(RefPtr<RenderPass> renderPass) override;
            UniPtr<Image> CreateImage(const ImageDescription& description, ImageUsageFlags usages) override;
            UniPtr<ShaderProgram> CreateShaderProgram(
                    const std::string& name, 
                    const ShaderConfig* config,
                    unsigned char* vert,
                    uint32_t vertSize,
                    unsigned char* frag,
                    uint32_t fragSize) override;
            
            RefPtr<VKSharedResource> GetSharedResource() { return sharedResource; }

        private:
            VKInstance* instance;
            VKDevice* device;
            VKAppWindow* appWindow;
            VKSurface* surface;
            VKPhysicalDevice* gpu;
            VKSwapChain* swapchain;

            VKMemAllocator* memAllocator;
            VKObjectManager* objectManager;

            VkDevice device_vk;
            UniPtr<VKContext> context;
            UniPtr<VKSharedResource> sharedResource;
            UniPtr<VKSwapChainImageProxy> swapChainImageProxy;
            UniPtr<VKDescriptorPoolCache> descriptorPoolCache;
            VkCommandPool commandPool;
            VkCommandBuffer renderingCmdBuf;
            VkCommandBuffer resourceCmdBuf;
            VKRenderTarget* finalRenderTarget;
            RefPtr<const DeviceQueue> mainQueue;

            struct InFlightFrame
            {
                uint32_t imageIndex = -1;
                VkSemaphore imageAcquireSemaphore = VK_NULL_HANDLE;
                VkSemaphore renderingFinishedSemaphore = VK_NULL_HANDLE;
                VkFence mainQueueFinishedFence = VK_NULL_HANDLE;
            } inFlightFrame;

            std::vector<UniPtr<VKCommandBuffer>> pendingCmdBufs;
    };
}
