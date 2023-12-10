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
class VKContext;
struct VKDescriptorPoolCache;
class VKDriver : public Gfx::GfxDriver
{
public:
    VKDriver(const CreateInfo& createInfo);
    ~VKDriver() override;

    void ForceSyncResources() override;
    void WaitForIdle() override;
    void QueueSubmit(
        RefPtr<CommandQueue> queue,
        std::span<Gfx::CommandBuffer*> cmdBufs,
        std::span<RefPtr<Semaphore>> waitSemaphores,
        std::span<Gfx::PipelineStageFlags> waitDstStageMasks,
        std::span<RefPtr<Semaphore>> signalSemaphroes,
        RefPtr<Fence> signalFence
    ) override;
    RefPtr<Semaphore> Present(std::vector<RefPtr<Semaphore>>&& semaphores) override;
    void WaitForFence(std::vector<RefPtr<Fence>>&& fence, bool waitAll, uint64_t timeout) override;
    AcquireNextSwapChainImageResult AcquireNextSwapChainImage(RefPtr<Semaphore> imageAcquireSemaphore) override;
    const GPUFeatures& GetGPUFeatures() override
    {
        return gpuFeatures;
    }

    bool IsFormatAvaliable(ImageFormat format, ImageUsageFlags usages) override;
    ;
    RefPtr<CommandQueue> GetQueue(QueueType flags) override;
    SDL_Window* GetSDLWindow() override;
    Image* GetSwapChainImage() override;
    Extent2D GetSurfaceSize() override;
    Backend GetGfxBackendType() override;
    RefPtr<VKSharedResource> GetSharedResource()
    {
        return sharedResource;
    }

    virtual UniPtr<Semaphore> CreateSemaphore(const Semaphore::CreateInfo& createInfo) override;
    virtual UniPtr<Fence> CreateFence(const Fence::CreateInfo& createInfo) override;
    UniPtr<Buffer> CreateBuffer(const Buffer::CreateInfo& createInfo) override;
    std::unique_ptr<ShaderResource> CreateShaderResource() override;
    std::unique_ptr<ImageView> CreateImageView(const ImageView::CreateInfo& createInfo) override;
    UniPtr<RenderPass> CreateRenderPass() override;
    UniPtr<FrameBuffer> CreateFrameBuffer(RefPtr<RenderPass> renderPass) override;
    UniPtr<Image> CreateImage(const ImageDescription& description, ImageUsageFlags usages) override;
    UniPtr<ShaderProgram> CreateShaderProgram(
        const std::string& name,
        std::shared_ptr<const ShaderConfig> config,
        const unsigned char* vert,
        uint32_t vertSize,
        const unsigned char* frag,
        uint32_t fragSize
    ) override;

    std::unique_ptr<ShaderProgram> CreateComputeShaderProgram(
        const std::string& name, std::shared_ptr<const ShaderConfig> config, const std::vector<uint32_t>& comp
    ) override;

    std::unique_ptr<ShaderProgram> CreateShaderProgram(
        const std::string& name,
        std::shared_ptr<const ShaderConfig> config,
        const std::vector<uint32_t>& vert,
        const std::vector<uint32_t>& frag
    ) override;
    UniPtr<CommandPool> CreateCommandPool(const CommandPool::CreateInfo& createInfo) override;

    void ClearResources() override;

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
    VKSwapChainImage* swapChainImage;
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
