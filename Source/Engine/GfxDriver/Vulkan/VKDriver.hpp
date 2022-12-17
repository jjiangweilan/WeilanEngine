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
#include "VKSemaphore.hpp"

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
    struct VKDescriptorPoolCache;
    class VKDriver : public Gfx::GfxDriver
    {
        public:
            VKDriver();
            ~VKDriver() override;

            void ExecuteCommandBuffer(UniPtr<CommandBuffer>&& cmdBuf) override;
            void ForceSyncResources() override;
            void WaitForIdle() override;
            void PrepareFrameResources(RefPtr<CommandQueue> queue) override;
            void QueueSubmit(RefPtr<CommandQueue> queue,
                    std::vector<RefPtr<CommandBuffer>>& cmdBufs,
                    std::vector<RefPtr<Semaphore>>& waitSemaphores,
                    std::vector<Gfx::PipelineStageFlags>& waitDstStageMasks,
                    std::vector<RefPtr<Semaphore>>& signalSemaphroes,
                    RefPtr<Fence> signalFence
                    ) override;
            RefPtr<Semaphore> Present(std::vector<RefPtr<Semaphore>> semaphores) override;
            void AcquireNextSwapChainImage(RefPtr<Semaphore> imageAcquireSemaphore) override;

            RefPtr<CommandQueue>     GetQueue(QueueType flags) override;
            SDL_Window*              GetSDLWindow() override;
            RefPtr<Image>            GetSwapChainImageProxy() override;
            Extent2D                 GetWindowSize() override;
            Backend                  GetGfxBackendType() override;
            RefPtr<VKSharedResource> GetSharedResource() { return sharedResource; }

            virtual UniPtr<Semaphore> CreateSemaphore(const Semaphore::CreateInfo& createInfo) override;
            virtual UniPtr<Fence>     CreateFence(const Fence::CreateInfo& createInfo) override;
            UniPtr<CommandBuffer>     CreateCommandBuffer() override;
            UniPtr<Buffer>         CreateBuffer(const Buffer::CreateInfo& createInfo) override;
            UniPtr<ShaderResource>    CreateShaderResource(RefPtr<ShaderProgram> shader, ShaderResourceFrequency frequency) override;
            UniPtr<RenderPass>        CreateRenderPass() override;
            UniPtr<FrameBuffer>       CreateFrameBuffer(RefPtr<RenderPass> renderPass) override;
            UniPtr<Image>             CreateImage(const ImageDescription& description, ImageUsageFlags usages) override;
            UniPtr<ShaderProgram>     CreateShaderProgram(
                    const std::string& name, 
                    const ShaderConfig* config,
                    unsigned char* vert,
                    uint32_t vertSize,
                    unsigned char* frag,
                    uint32_t fragSize) override;

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
            UniPtr<VKSwapChainImageProxy> swapChainImageProxy;
            UniPtr<VKContext> context;
            UniPtr<VKSharedResource> sharedResource;
            UniPtr<VKDescriptorPoolCache> descriptorPoolCache;
            VkCommandPool commandPool;
            VkCommandBuffer cmdBufs[2];
#define renderingCmdBuf cmdBufs[0]
#define resourceCmdBuf cmdBufs[1]
            VKRenderTarget* finalRenderTarget;
            RefPtr<const VKCommandQueue> mainQueue;
            RefPtr<const VKCommandQueue> graphics0queue;

            struct InFlightFrame
            {
                UniPtr<VKSemaphore> imageAcquireSemaphore;
            } inFlightFrame;

            std::vector<UniPtr<VKCommandBuffer>> pendingCmdBufs;
    };
}
