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
    const GPUFeatures& GetGPUFeatures() override
    {
        return gpuFeatures;
    }

    bool IsFormatAvaliable(ImageFormat format, ImageUsageFlags usages) override;
    ;
    SDL_Window* GetSDLWindow() override;
    Image* GetSwapChainImage() override;
    Extent2D GetSurfaceSize() override;
    Backend GetGfxBackendType() override;
    void Present() override;
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

    // RHI implementation
    void ExecuteImmediately(std::function<void(Gfx::CommandBuffer& cmd)>&& f) override;
    void Schedule(std::function<void(Gfx::CommandBuffer& cmd)>&& f) override;
    void UploadBuffer(
        Gfx::Buffer& dst,
        uint8_t* data,
        size_t size,
        size_t dstOffset = 0,
        std::function<void()> finishCallback = nullptr
    ) override;
    void UploadImage(
        Gfx::Image& dst,
        uint8_t* data,
        size_t size,
        uint32_t mipLevel,
        uint32_t arayLayer,
        Gfx::ImageAspect aspect,
        std::function<void()> finishedCallback = nullptr
    ) override;
    void GenerateMipmaps(Gfx::Image& image) override
    {
        GenerateMipmaps(static_cast<VKImage&>(image));
    }

    void GenerateMipmaps(VKImage& image);

public:
    std::unique_ptr<VKMemAllocator> memAllocator;
    std::unique_ptr<VKObjectManager> objectManager;

    std::unique_ptr<VKContext> context;
    std::unique_ptr<VKSharedResource> sharedResource;
    std::unique_ptr<VKDescriptorPoolCache> descriptorPoolCache;
    VkCommandPool mainCmdPool;

    struct DriverConfig
    {
        int swapchainImageCount = 3;
    } driverConfig;

    struct Instance
    {
        VkInstance handle;
        VkDebugUtilsMessengerEXT debugMessenger;
    } instance;

    struct Device
    {
        VkDevice handle;
    } device;

    Queue mainQueue;

    GPU gpu;
    Swapchain swapchain;

    struct Surface
    {
        VkSurfaceKHR handle;
        VkSurfaceCapabilitiesKHR surfaceCapabilities;
        std::vector<VkPresentModeKHR> surfacePresentModes;
        std::vector<VkSurfaceFormatKHR> surfaceFormats;
    } surface;

    struct AppWidnow
    {
        SDL_Window* handle;
        Extent2D size;
    } window;

    GPUFeatures gpuFeatures;

    struct PendingBufferUpload
    {
        VkBuffer dst;
        uint8_t* data;
        size_t size;
        size_t dstOffset;
        std::function<void()> finishCallback;
    };

    struct PendingImageUpload
    {
        VKImage* dst;
        uint8_t* data;
        size_t size;
        uint32_t mipLevel;
        uint32_t arrayLayer;
        VkImageAspectFlags aspect;
        std::function<void()> finishCallback;
    };

    struct Buffer
    {
        VkBuffer handle = VK_NULL_HANDLE;
        VmaAllocation allocation;
        VmaAllocationInfo allocationInfo;
        size_t size;
        bool inUse;
    };

    // RHI implementation
    struct InflightData
    {
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkFence cmdFence = VK_NULL_HANDLE;
        VkSemaphore cmdSemaphore;
        VkSemaphore presentSemaphore;
        uint32_t swapchainIndex;
        VKDriver::Buffer* stagingBuffer;

        // inflight data needs to notify all uploads when it's command buffer is finished
        std::vector<PendingBufferUpload> pendingBufferUploads;
        std::vector<PendingImageUpload> pendingImageUploads;
    };
    std::vector<InflightData> inflightData = {};
    uint32_t currentInflightIndex = 0;
    std::unique_ptr<VKSwapChainImage> swapchainImage = nullptr;
    std::vector<std::function<void(VkCommandBuffer&)>> internalPendingCommands = {};
    std::vector<std::function<void(Gfx::CommandBuffer&)>> pendingCommands = {};

    VkCommandBuffer immediateCmd = VK_NULL_HANDLE;
    VkFence immediateCmdFence = VK_NULL_HANDLE;

    // buffer uploads
    std::vector<PendingBufferUpload> nextBufferUploads = {};
    size_t nextBufferUploadsSize = 0;

    // image uploads
    std::vector<PendingImageUpload> nextImageUploads = {};
    size_t nextImageUploadsSize = 0;

    std::vector<std::unique_ptr<Buffer>> stagingBuffers;

    void CreateInstance();
    void CreatePhysicalDevice();
    void CreateDevice();
    void CreateAppWindow();
    void CreateSurface();
    bool CreateOrOverrideSwapChain();

    Buffer Driver_CreateBuffer(size_t size, VkBufferUsageFlags usage, VmaAllocationCreateFlags vmaCreateFlags);
    void Driver_DestroyBuffer(Buffer* b);

    std::vector<const char*> AppWindowGetRequiredExtensions();
    bool Instance_CheckAvalibilityOfValidationLayers(const std::vector<const char*>& validationLayers);
    bool Swapchain_GetImagesFromVulkan();

    void RHI_UploadData(VkCommandBuffer cmd);

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
};
} // namespace Gfx
