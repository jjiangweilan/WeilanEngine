#pragma once

#include "Buffer.hpp"
#include "CommandBuffer.hpp"
#include "CommandPool.hpp"
#include "CommandQueue.hpp"
#include "Fence.hpp"
#include "Image.hpp"
#include "ImageView.hpp"
#include "Libs/EnumFlags.hpp"
#include "Libs/Ptr.hpp"
#include "Semaphore.hpp"

#include <SDL2/SDL.h>
#include <glm/glm.hpp>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

#if defined(_WIN32) || defined(_WIN64)
#undef CreateSemaphore
#endif

namespace Engine::Gfx
{
enum class Backend
{
    Vulkan,
    OpenGL
};

struct GPUFeatures
{
    bool textureCompressionETC2 = false;
    bool textureCompressionBC = true;
    bool textureCompressionASTC4x4 = false;
};

enum class AcquireNextSwapChainImageResult
{
    Succeeded,
    Failed,
    Recreated
};

class GfxDriver
{
public:
    struct CreateInfo
    {
        // the initial window size used to create sdl window
        Extent2D windowSize;
    };

    static RefPtr<GfxDriver> Instance();

    static std::unique_ptr<GfxDriver> CreateGfxDriver(Backend backend, const CreateInfo& createInfo);

    virtual ~GfxDriver(){};

    virtual bool IsFormatAvaliable(ImageFormat format, ImageUsageFlags uages) = 0;
    virtual const GPUFeatures& GetGPUFeatures() = 0;
    virtual Image* GetSwapChainImage() = 0;
    virtual RefPtr<CommandQueue> GetQueue(QueueType flags) = 0;
    virtual SDL_Window* GetSDLWindow() = 0;
    virtual Backend GetGfxBackendType() = 0;
    virtual Extent2D GetSurfaceSize() = 0;

    virtual UniPtr<CommandPool> CreateCommandPool(const CommandPool::CreateInfo& createInfo) = 0;
    virtual std::unique_ptr<ImageView> CreateImageView(const ImageView::CreateInfo& createInfo) = 0;
    virtual UniPtr<Buffer> CreateBuffer(const Buffer::CreateInfo& createInfo) = 0;
    virtual UniPtr<ShaderResource> CreateShaderResource(
        RefPtr<ShaderProgram> shader, ShaderResourceFrequency frequency
    ) = 0;
    virtual UniPtr<RenderPass> CreateRenderPass() = 0;
    virtual UniPtr<FrameBuffer> CreateFrameBuffer(RefPtr<RenderPass> renderPass) = 0;
    virtual UniPtr<Image> CreateImage(const ImageDescription& description, ImageUsageFlags usages) = 0;
    virtual UniPtr<ShaderProgram> CreateShaderProgram(
        const std::string& name,
        std::shared_ptr<const ShaderConfig> config,
        const unsigned char* vert,
        uint32_t vertSize,
        const unsigned char* frag,
        uint32_t fragSize
    ) = 0;

    virtual std::unique_ptr<ShaderProgram> CreateShaderProgram(
        const std::string& name,
        std::shared_ptr<const ShaderConfig> config,
        const std::vector<uint32_t>& vert,
        const std::vector<uint32_t>& frag
    ) = 0;

    virtual std::unique_ptr<ShaderProgram> CreateComputeShaderProgram(
        const std::string& name, std::shared_ptr<const ShaderConfig> config, const std::vector<uint32_t>& comp
    ) = 0;

    virtual UniPtr<Semaphore> CreateSemaphore(const Semaphore::CreateInfo& createInfo) = 0;
    virtual UniPtr<Fence> CreateFence(const Fence::CreateInfo& createInfo) = 0;

    virtual void QueueSubmit(
        RefPtr<CommandQueue> queue,
        std::span<Gfx::CommandBuffer*> cmdBufs,
        std::span<RefPtr<Semaphore>> waitSemaphores,
        std::span<Gfx::PipelineStageFlags> waitDstStageMasks,
        std::span<RefPtr<Semaphore>> signalSemaphroes,
        RefPtr<Fence> signalFence
    ) = 0;
    virtual void ForceSyncResources() = 0;
    virtual void WaitForIdle() = 0;
    virtual RefPtr<Semaphore> Present(std::vector<RefPtr<Semaphore>>&& semaphores) = 0;

    // return true if swapchain is recreated
    virtual AcquireNextSwapChainImageResult AcquireNextSwapChainImage(RefPtr<Semaphore> imageAcquireSemaphore) = 0;
    virtual void WaitForFence(std::vector<RefPtr<Fence>>&& fence, bool waitAll, uint64_t timeout) = 0;

    virtual void ClearResources() = 0;

private:
    static GfxDriver*& InstanceInternal();
};
} // namespace Engine::Gfx

namespace Engine
{
inline RefPtr<Gfx::GfxDriver> GetGfxDriver()
{
    return Gfx::GfxDriver::Instance();
}
} // namespace Engine
