#pragma once
#include <SDL2/SDL.h>
#include <glm/glm.hpp>
#include <memory>
#include <unordered_map>
#include <vector>
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "../GfxDriver.hpp"

#include "VKCommandBuffer.hpp"
#include "VKCommonDefinations.hpp"

#include "VKCommandPool.hpp"
#include "VKRenderTarget.hpp"
#include "VKSemaphore.hpp"
#include "VKShaderProgram.hpp"

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
    VKDriver(const CreateInfo& createInfo);
    ~VKDriver() override;

    void ForceSyncResources() override;
    void WaitForIdle() override;
    void QueueSubmit(RefPtr<CommandQueue> queue,
                     std::span<RefPtr<CommandBuffer>> cmdBufs,
                     std::span<RefPtr<Semaphore>> waitSemaphores,
                     std::span<Gfx::PipelineStageFlags> waitDstStageMasks,
                     std::span<RefPtr<Semaphore>> signalSemaphroes,
                     RefPtr<Fence> signalFence) override;
    RefPtr<Semaphore> Present(std::vector<RefPtr<Semaphore>>&& semaphores) override;
    void WaitForFence(std::vector<RefPtr<Fence>>&& fence, bool waitAll, uint64_t timeout) override;
    bool AcquireNextSwapChainImage(RefPtr<Semaphore> imageAcquireSemaphore) override;
    const GPUFeatures& GetGPUFeatures() override { return gpuFeatures; }

    bool IsFormatAvaliable(ImageFormat format, ImageUsageFlags usages) override;
    ;
    RefPtr<CommandQueue> GetQueue(QueueType flags) override;
    SDL_Window* GetSDLWindow() override;
    RefPtr<Image> GetSwapChainImageProxy() override;
    Extent2D GetSurfaceSize() override;
    Backend GetGfxBackendType() override;
    RefPtr<VKSharedResource> GetSharedResource() { return sharedResource; }

    virtual UniPtr<Semaphore> CreateSemaphore(const Semaphore::CreateInfo& createInfo) override;
    virtual UniPtr<Fence> CreateFence(const Fence::CreateInfo& createInfo) override;
    UniPtr<Buffer> CreateBuffer(const Buffer::CreateInfo& createInfo) override;
    UniPtr<ShaderResource> CreateShaderResource(RefPtr<ShaderProgram> shader,
                                                ShaderResourceFrequency frequency) override;
    UniPtr<RenderPass> CreateRenderPass() override;
    UniPtr<FrameBuffer> CreateFrameBuffer(RefPtr<RenderPass> renderPass) override;
    UniPtr<Image> CreateImage(const ImageDescription& description, ImageUsageFlags usages) override;
    UniPtr<ShaderProgram> CreateShaderProgram(const std::string& name,
                                              const ShaderConfig* config,
                                              unsigned char* vert,
                                              uint32_t vertSize,
                                              unsigned char* frag,
                                              uint32_t fragSize) override;
    UniPtr<CommandPool> CreateCommandPool(const CommandPool::CreateInfo& createInfo) override;

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
    RefPtr<VKCommandQueue> mainQueue;

    UniPtr<VKCommandPool> commandPool;

    struct InFlightFrame
    {
        UniPtr<VKSemaphore> imageAcquireSemaphore;
    } inFlightFrame;
    GPUFeatures gpuFeatures;
};
} // namespace Engine::Gfx
