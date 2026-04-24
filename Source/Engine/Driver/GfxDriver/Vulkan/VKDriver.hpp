#pragma once
#include "Engine/Driver/GfxDriver/Vulkan/RayTracing/VKRayTracing.hpp"
#include "Engine/Driver/GfxDriver/Vulkan/VKCommon.hpp"
#include "Engine/Library/DynamicArray.hpp"
#include <SDL.h>
#include <glm/glm.hpp>
#include <memory>
#include <unordered_map>

#include "../GfxDriver.hpp"

#include "Engine/Driver/GfxDriver/Vulkan/VKWindow.hpp"
#include "VKCommandBuffer.hpp"
#include "VKCommandBufferProcessor.hpp"
#include "VKCommonDefinations.hpp"

#include "VKCommandPool.hpp"
#include "VKInflightCmd.hpp"
#include "VKRenderTarget.hpp"
#include "VKSemaphore.hpp"
#include "VKShaderProgram.hpp"

#include "Engine/Library/ArenaAllocator.hpp"
#include "VKRawBuffer.hpp"

namespace Gfx
{
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
class VKDataUploader;
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
    void WaitForFence(std::vector<RefPtr<Fence>>&& fence, bool waitAll, uint64_t timeout) override;
    const GPUFeatures& GetGPUFeatures() override { return gpuFeatures; }

    bool IsFormatAvaliable(GfxFormat format, ImageUsageFlags usages) override;
    ;
    SDL_Window* GetSDLWindow() override;
    Image* GetSwapChainImage() override;
    Extent2D GetSurfaceSize() override;
    Backend GetGfxBackendType() override;
    RefPtr<VKSharedResource> GetSharedResource() { return sharedResource; }

    virtual std::unique_ptr<Semaphore> CreateSemaphore(const Semaphore::CreateInfo& createInfo) override;
    virtual std::unique_ptr<Fence> CreateFence(const Fence::CreateInfo& createInfo) override;
    std::unique_ptr<Buffer> CreateBuffer(const Buffer::CreateInfo& createInfo) override;
    std::unique_ptr<ShaderResource> CreateShaderResource() override;
    std::unique_ptr<Sampler> CreateSampler(const Sampler::CreateInfo& createInfo) override;
    std::unique_ptr<ImageView> CreateImageView(const ImageView::CreateInfo& createInfo) override;
    std::unique_ptr<RenderPass_Deprecated> CreateRenderPass() override;
    std::unique_ptr<FrameBuffer> CreateFrameBuffer(RefPtr<RenderPass_Deprecated> renderPass) override;
    std::unique_ptr<Image> CreateImage(const ImageDescription& description, ImageUsageFlags usages) override;
    Window* CreateExtraWindow(SDL_Window* window) override;
    void DestroyExtraWindow(Window* window) override;
    std::unique_ptr<CommandBuffer> CreateCommandBuffer() override;

    std::unique_ptr<RayTracingContext> CreateRayTracingContext() override;

    void CaptureFrameRenderDoc(bool nextFrame) override;

    std::unique_ptr<ShaderProgram> CreateShaderProgram(PipelineCreateInfo& createInfo) override;
    std::unique_ptr<CommandPool> CreateCommandPool(const CommandPool::CreateInfo& createInfo) override;
    void ExecuteCommandBuffer(Gfx::CommandBuffer& cmd) override;
    void ExecuteCommandBufferImmediately(Gfx::CommandBuffer& cmd) override;

    void ClearResources() override;

    // Append a request to upload data to GPU before this frame's rendering happens, data is directly copied to a
    // staging buffer internally, client don't need to manage the data memory after calling this function
    void UploadBuffer(const Gfx::Buffer& dst, uint8_t* data, size_t size, size_t dstOffset = 0) override;
    void UploadImage(
        Gfx::Image& dst, uint8_t* data, size_t size, uint32_t mipLevel, uint32_t arrayLayer, Gfx::ImageAspect aspect
    ) override;

    void UploadImage(
        Gfx::Image& dst,
        uint8_t* data,
        size_t size,
        uint32_t mipLevel,
        uint32_t arrayLayer,
        Gfx::ImageAspect aspect,
        VkImageLayout finalLayout
    );

    void ShaderReloaded() override;
    void SetGPUProfilerEnabled(bool enabled) override;
    const IProfiler& GetGPUProfiler() override { return profiler; }

    void GenerateMipmaps(Gfx::Image& image) override
    {
        GenerateMipmaps(static_cast<VKImage&>(image).GetSRef<VKImage>());
    }
    void InitGfxImage(Gfx::Image& image, glm::vec4 color) override;
    bool BeginFrame() override;
    bool EndFrame() override;

    void GenerateMipmaps(SRef<VKImage> image);

    Gfx::Image* GetImageFromRenderGraph(const Gfx::ImageIdentifier& id) override;

    void SetWin32WindowInteropTexture(const void* sharedHandle, int2 size) override;
    void UnsetWin32WindowInteropTexture(int2 size) override;

    void FlushPendingCommands() override;

public:
    std::unique_ptr<VKMemAllocator> memAllocator;
    std::unique_ptr<VKObjectManager> objectManager;

    std::unique_ptr<VKContext> context;
    std::unique_ptr<VKSharedResource> sharedResource;
    std::unique_ptr<VKDescriptorPoolCache> descriptorPoolCache;
    std::unique_ptr<VKDataUploader> dataUploader;
    VkCommandPool mainCmdPool;

    struct Instance
    {
        VkInstance handle;
        VkDebugUtilsMessengerEXT debugMessenger;
    } instance;

    struct Device
    {
        VkDevice handle;
    } device;

    VkPhysicalDeviceFeatures deviceFeatures{.independentBlend = true, .multiDrawIndirect = true, .fillModeNonSolid = true, .shaderImageGatherExtended = true};

    Queue mainQueue;
    GPU gpu;
    Swapchain swapchain;
    Surface surface;
    GPUFeatures gpuFeatures;
    GfxFeaturesSettings featureSettings;
    std::vector<std::unique_ptr<VKWindow>> extraWindows{};

    std::mutex driverMutex;

    std::vector<VKFrameContext> frameContexts = {};
    std::vector<VkSemaphore> imageAcquireSemaphores = {};
    std::vector<VkSemaphore> presentSemaphores = {};

    VKFramePrepareData framePrepareData;
    uint32_t currentInflightIndex = 0;

    std::vector<std::function<void(VkCommandBuffer&)>> internalPendingCommands = {};
    VkSemaphore transferSignalSemaphore;
    VkSemaphore dataUploaderWaitSemaphore = VK_NULL_HANDLE;
    bool firstFrame = true;
    size_t frameCount = 0;
    std::unique_ptr<VKCommandBufferProcessor> commandBufferProcessor;

    VkCommandBuffer immediateCmd = VK_NULL_HANDLE;
    VkFence immediateCmdFence = VK_NULL_HANDLE;

    bool needPresent = true;

    void CreateInstance(bool enableValidationLayers);
    void CreatePhysicalDevice();
    void CreateDevice();
    void CreateSurface();

    VKRawBuffer Driver_CreateBuffer(size_t size, VkBufferUsageFlags usage, VmaAllocationCreateFlags vmaCreateFlags);
    void Driver_DestroyBuffer(VKRawBuffer& b);
    bool Present(
        VkSemaphore presentSemaphore,
        VkSwapchainKHR swapChainHandle,
        Surface& surface,
        Swapchain& swapchain,
        uint32_t swapchainIndex
    );

    std::vector<const char*> AppWindowGetRequiredExtensions();
    bool Instance_CheckAvalibilityOfValidationLayers(const std::vector<const char*>& validationLayers);

    void AppendOnCompleteCallback(const std::function<void()>& callback);

private:
    SDL_Window* window;
    struct SDLInfo;
    std::unique_ptr<SDLInfo> sdlInfo;
    std::unique_ptr<VKRayTracing::Manager> rayTracingManager;

    // ====== profiler ======
    struct TimestampQuery
    {
        uint64_t timestamp;
    };
    std::vector<TimestampQuery> timestamps;
    Profiler profiler;

    bool captureFrame = false;
    bool captureFrameBegin = false;

    void FrameEndClear();

    VkResult CreateDebugUtilsMessengerEXT(
        VkInstance instance,
        const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkDebugUtilsMessengerEXT* pDebugMessenger
    );

    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData
    );

    ArenaAllocator<1024> allocator;
    void WaitForCurrentInflightCmd();
    void QueryGPUTimestamp(CmdBufExecutionReport& execReport);

    void BeginFrameCapture();
};
} // namespace Gfx
