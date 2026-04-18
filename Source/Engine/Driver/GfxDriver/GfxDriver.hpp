#pragma once

#include "Buffer.hpp"
#include "CommandBuffer.hpp"
#include "CommandPool.hpp"
#include "CommandQueue.hpp"
#include "CompiledSpv.hpp"
#include "Engine/Core/Profiler/Profiler.hpp"
#include "Engine/Core/Ptr.hpp"
#include "Engine/Library/EnumFlags.hpp"
#include "Engine/ThirdParty/renderdoc/renderdoc_app.h"
#include "Fence.hpp"
#include "Image.hpp"
#include "ImageView.hpp"
#include "RayTracingContext.hpp"
#include "Sampler.hpp"
#include "Semaphore.hpp"
#include "Window.hpp"

#include "Engine/Library/DynamicArray.hpp"
#include <SDL.h>
#include <glm/glm.hpp>
#include <memory>
#include <span>

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

    bool multiDrawIndirect = false;
    bool timestampPeriod = false;
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
        SDL_Window* window = nullptr;
        bool enableRenderDoc = false;
        bool enableGfxDriverValidation = false;
        bool enableGPUTimestamp = false;
        int gpuTimestampQueryMaxCount = 128;
    };

    static RefPtr<GfxDriver> Instance();

    static std::unique_ptr<GfxDriver> CreateGfxDriver(Backend backend, const CreateInfo& createInfo);

    GfxDriver();
    virtual ~GfxDriver();

    // TODO(perf):
    // 1. GPU profiler should be able to turn off when not needed
    virtual void SetGPUProfilerEnabled(bool enabled) = 0;
    virtual const IProfiler& GetGPUProfiler() = 0;
    virtual bool IsFormatAvaliable(GfxFormat format, ImageUsageFlags uages) = 0;
    virtual const GPUFeatures& GetGPUFeatures() = 0;
    virtual Image* GetSwapChainImage() = 0;
    virtual SDL_Window* GetSDLWindow() = 0;
    virtual Backend GetGfxBackendType() = 0;
    virtual Extent2D GetSurfaceSize() = 0;

    // virtual const Profiler& GetFrameProfiler() const {return {};}
    virtual bool BeginFrame() = 0;

    // return true if swapchain recreated
    virtual bool EndFrame() = 0;
    virtual std::unique_ptr<CommandPool> CreateCommandPool(const CommandPool::CreateInfo& createInfo) = 0;
    virtual std::unique_ptr<ImageView> CreateImageView(const ImageView::CreateInfo& createInfo) = 0;
    virtual std::unique_ptr<Buffer> CreateBuffer(const Buffer::CreateInfo& createInfo) = 0;
    virtual std::unique_ptr<ShaderResource> CreateShaderResource() = 0;
    virtual std::unique_ptr<Sampler> CreateSampler(const Sampler::CreateInfo& createInfo) = 0;
    virtual std::unique_ptr<RenderPass_Deprecated> CreateRenderPass() = 0;
    virtual std::unique_ptr<FrameBuffer> CreateFrameBuffer(RefPtr<RenderPass_Deprecated> renderPass) = 0;
    virtual std::unique_ptr<Image> CreateImage(const ImageDescription& description, ImageUsageFlags usages) = 0;
    virtual std::unique_ptr<ShaderProgram> CreateShaderProgram(PipelineCreateInfo& createInfo) = 0;

    virtual std::unique_ptr<Semaphore> CreateSemaphore(const Semaphore::CreateInfo& createInfo) = 0;
    virtual std::unique_ptr<Fence> CreateFence(const Fence::CreateInfo& createInfo) = 0;

    virtual std::unique_ptr<CommandBuffer> CreateCommandBuffer() = 0;

    virtual std::unique_ptr<RayTracingContext> CreateRayTracingContext() = 0;

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

    /**
     * @brief replace the swapchain image with the interop texture, this texture will be presented to screen, the format will always be r8g8b8a8_unorm
     *
     * @param sharedHandle the HANDLE
     * @param size the size of the interop texture
     */
    virtual void SetWin32WindowInteropTexture(const void* sharedHandle, int2 size) = 0;
    virtual void UnsetWin32WindowInteropTexture(int2 size) = 0;

    virtual void ExecuteCommandBufferImmediately(Gfx::CommandBuffer& cmd) = 0;
    virtual void ExecuteCommandBuffer(Gfx::CommandBuffer& cmd) = 0;
    virtual void UploadBuffer(const Gfx::Buffer& dst, uint8_t* data, size_t size, size_t dstOffset = 0) = 0;
    virtual void ShaderReloaded() = 0;

    // use this with caution, the image may be destroyed and recreated everyframe for the same id
    virtual Gfx::Image* GetImageFromRenderGraph(const Gfx::ImageIdentifier& id) = 0;

    virtual void UploadImage(
        Gfx::Image& dst,
        uint8_t* data,
        size_t size,
        uint32_t mipLevel = 0,
        uint32_t arayLayer = 0,
        Gfx::ImageAspect aspect = Gfx::ImageAspect::Color
    ) = 0;
    virtual void GenerateMipmaps(Gfx::Image& image) = 0;

    // Clears the image to the given color and transitions it to SHADER_READ_ONLY_OPTIMAL.
    // Useful for initializing history images (e.g. GI temporal buffers) on the first frame.
    virtual void InitGfxImage(Gfx::Image& image, glm::vec4 color) = 0;

    virtual Window* CreateExtraWindow(SDL_Window* window) = 0;
    virtual void DestroyExtraWindow(Window* window) = 0;

    std::unique_ptr<Buffer> CreateBuffer(
        size_t size,
        BufferUsageFlags usages,
        bool visibleInCPU = false,
        bool gpuWrite = false,
        const char* debugName = ""
    );

    bool IsRenderDocInitialized() const { return renderDocAPI != nullptr; }
    void InitializeRenderDoc(bool enableValidation = false);
    virtual void CaptureFrameRenderDoc(bool nextFrame) = 0;

private:
    static GfxDriver*& InstanceInternal();

protected:
    RENDERDOC_API_1_6_0* renderDocAPI = nullptr;
    struct RenderdocModule;
    std::unique_ptr<RenderdocModule> renderdocModule;
};
} // namespace Gfx

inline RefPtr<Gfx::GfxDriver> GetGfxDriver()
{
    return Gfx::GfxDriver::Instance();
}
