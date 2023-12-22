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
    RefPtr<CommandQueue> GetQueue(QueueType flags) override;
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
    void Schedule(std::function<void(Gfx::CommandBuffer& cmd)>&& f) override;
    void Render() override;
    void UploadBuffer(Gfx::Buffer& dst, uint8_t* data, size_t size, size_t dstOffset = 0) override;
    void UploadImage(Gfx::Image& dst, uint8_t* data, size_t size, uint32_t mipLevel = 0, uint32_t arayLayer = 0)
        override;
    void GenerateMipmaps(Gfx::Image& image) override
    {
        GenerateMipmaps(static_cast<VKImage&>(image));
    }

    void UploadImage(VkImage image, uint8_t* data, size_t size, uint32_t mipLevel, uint32_t arayLayer);
    void GenerateMipmaps(VKImage& image);

private:
    VKMemAllocator* memAllocator;
    VKObjectManager* objectManager;

    VkDevice device_vk;
    UniPtr<VKContext> context;
    UniPtr<VKSharedResource> sharedResource;
    UniPtr<VKDescriptorPoolCache> descriptorPoolCache;
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

    struct Queue
    {
        VkQueue handle;
        uint32_t queueIndex;
        uint32_t queueFamilyIndex;
    };
    Queue mainQueue;

    struct GPU
    {
        VkPhysicalDevice handle;

        VkPhysicalDeviceMemoryProperties memProperties;
        VkPhysicalDeviceProperties physicalDeviceProperties{};
        VkPhysicalDeviceFeatures physicalDeviceFeatures{};

        std::vector<VkQueueFamilyProperties> queueFamilyProperties;
        std::vector<VkExtensionProperties> availableExtensions;
    } gpu;

    struct Swapchain
    {
        VkSwapchainKHR handle;
        VkSurfaceFormatKHR surfaceFormat;
        VkExtent2D extent;
        VkImageUsageFlags imageUsageFlags;
        VkSurfaceTransformFlagBitsKHR surfaceTransform;
        VkPresentModeKHR presentMode;
        uint32_t numberOfImages;
    } swapchain;

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

    // RHI implementation
    struct InflightData
    {
        VkCommandBuffer cmd;
        VkFence cmdFence;
        VkSemaphore cmdSemaphore;
        VkSemaphore presentSemaphore;
        uint32_t swapchainIndex;
        std::vector<std::function<void(Gfx::CommandBuffer&)>> pendingCommands;
    };
    std::vector<InflightData> inflightData;
    uint32_t currentInflightIndex;
    std::unique_ptr<VKSwapChainImage> swapchainImage;
    VkBuffer stagingBuffer;

    void CreateInstance();
    void CreatePhysicalDevice();
    void CreateDevice();
    void CreateAppWindow();
    void CreateSurface();
    bool CreateOrOverrideSwapChain();

    std::vector<const char*> AppWindowGetRequiredExtensions();
    bool Instance_CheckAvalibilityOfValidationLayers(const std::vector<const char*>& validationLayers);
    bool Swapchain_GetImagesFromVulkan();

    void RHI_UploadReadonlyData(VkCommandBuffer cmd);

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
