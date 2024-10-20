#pragma once

#include "Buffer.hpp"
#include "CommandBuffer.hpp"
#include "CommandPool.hpp"
#include "CommandQueue.hpp"
#include "CompiledSpv.hpp"
#include "Fence.hpp"
#include "Image.hpp"
#include "ImageView.hpp"
#include "Libs/EnumFlags.hpp"
#include "Libs/Ptr.hpp"
#include "Semaphore.hpp"
#include "Window.hpp"

#include <SDL.h>
#include <glm/glm.hpp>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

#if defined(_WIN32) || defined(_WIN64)
#undef CreateSemaphore
#endif

namespace Gfx
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

enum class GfxEvent
{
    SwapchainRecreated
};

class GfxDriver
{
public:
    struct CreateInfo
    {
        // the initial window size used to create sdl window
        SDL_Window* window;
    };

    static RefPtr<GfxDriver> Instance();

    static std::unique_ptr<GfxDriver> CreateGfxDriver(Backend backend, const CreateInfo& createInfo);

    virtual ~GfxDriver(){};

    virtual bool IsFormatAvaliable(ImageFormat format, ImageUsageFlags uages) = 0;
    virtual const GPUFeatures& GetGPUFeatures() = 0;
    virtual Image* GetSwapChainImage() = 0;
    virtual SDL_Window* GetSDLWindow() = 0;
    virtual Backend GetGfxBackendType() = 0;
    virtual Extent2D GetSurfaceSize() = 0;

    virtual bool BeginFrame() = 0;

    // return true if swapchain recreated
    virtual bool EndFrame() = 0;
    virtual UniPtr<CommandPool> CreateCommandPool(const CommandPool::CreateInfo& createInfo) = 0;
    virtual std::unique_ptr<ImageView> CreateImageView(const ImageView::CreateInfo& createInfo) = 0;
    virtual UniPtr<Buffer> CreateBuffer(const Buffer::CreateInfo& createInfo) = 0;
    virtual std::unique_ptr<ShaderResource> CreateShaderResource() = 0;
    virtual UniPtr<RenderPass> CreateRenderPass() = 0;
    virtual UniPtr<FrameBuffer> CreateFrameBuffer(RefPtr<RenderPass> renderPass) = 0;
    virtual UniPtr<Image> CreateImage(const ImageDescription& description, ImageUsageFlags usages) = 0;
    virtual std::unique_ptr<ShaderProgram> CreateShaderProgram(
        const std::string& name, std::shared_ptr<const ShaderConfig> config, ShaderProgramCreateInfo& createInfo
    ) = 0;

    virtual UniPtr<Semaphore> CreateSemaphore(const Semaphore::CreateInfo& createInfo) = 0;
    virtual UniPtr<Fence> CreateFence(const Fence::CreateInfo& createInfo) = 0;

    virtual std::unique_ptr<CommandBuffer> CreateCommandBuffer() = 0;

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

    virtual void FlushPendingCommands() = 0;
    // return true if swapchain is recreated
    virtual void WaitForFence(std::vector<RefPtr<Fence>>&& fence, bool waitAll, uint64_t timeout) = 0;

    virtual void ClearResources() = 0;

    virtual void ExecuteCommandBufferImmediately(Gfx::CommandBuffer& cmd) = 0;
    virtual void ExecuteCommandBuffer(Gfx::CommandBuffer& cmd) = 0;
    virtual void UploadBuffer(Gfx::Buffer& dst, uint8_t* data, size_t size, size_t dstOffset = 0) = 0;

    // use this with caution, the image may be destroyed and recreated everyframe for the same id
    virtual Gfx::Image* GetImageFromRenderGraph(const Gfx::RG::ImageIdentifier& id) = 0;

    virtual void UploadImage(
        Gfx::Image& dst,
        uint8_t* data,
        size_t size,
        uint32_t mipLevel = 0,
        uint32_t arayLayer = 0,
        Gfx::ImageAspect aspect = Gfx::ImageAspect::Color
    ) = 0;
    virtual void GenerateMipmaps(Gfx::Image& image) = 0;

    virtual Window* CreateExtraWindow(SDL_Window* window) = 0;
    virtual void DestroyExtraWindow(Window* window) = 0;

    std::unique_ptr<Buffer> CreateBuffer(
        size_t size, BufferUsageFlags usages, bool visibleInCPU = false, bool gpuWrite = false, const char* debugName = ""
    );

private:
    static GfxDriver*& InstanceInternal();
};
} // namespace Gfx

inline RefPtr<Gfx::GfxDriver> GetGfxDriver()
{
    return Gfx::GfxDriver::Instance();
}
