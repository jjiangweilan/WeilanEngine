#include "VKDriver.hpp"

#include "Internal/VKEnumMapper.hpp"
#include "Internal/VKMemAllocator.hpp"
#include "Internal/VKObjectManager.hpp"
#include "VKBuffer.hpp"
#include "VKCommandPool.hpp"
#include "VKContext.hpp"
#include "VKDescriptorPool.hpp"
#include "VKExtensionFunc.hpp"
#include "VKFence.hpp"
#include "VKFrameBuffer.hpp"
#include "VKImageView.hpp"
#include "VKRenderPass.hpp"
#include "VKShaderModule.hpp"
#include "VKShaderResource.hpp"
#include "VKSharedResource.hpp"
#include <SDL2/SDL_vulkan.h>

#include <algorithm>
#include <set>
#include <spdlog/spdlog.h>
#include <string>
#if !_MSC_VER
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnullability-completeness"
#pragma GCC diagnostic ignored "-Wswitch"
#endif
#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

#if !_MSC_VER
#pragma GCC diagnostic pop
#endif
#if defined(_WIN32) || defined(_WIN64)
#undef CreateSemaphore
#endif

namespace Gfx
{
// I can't use the MakeUnique function in Ptr.hpp in this translation unit on Window machine. Not sure why.
// This is a temporary workaround
template <class T, class... Args>
UniPtr<T> MakeUnique1(Args&&... args)
{
    return UniPtr<T>(new T(std::forward<Args>(args)...));
}

VKDriver::VKDriver(const CreateInfo& createInfo)
{
    CreateAppWindow();
    CreateInstance();
    CreatePhysicalDevice();
    CreateSurface();
    CreateDevice();

    objectManager = std::make_unique<VKObjectManager>(device.handle);
    memAllocator =
        std::make_unique<VKMemAllocator>(instance.handle, device.handle, gpu.handle, mainQueue.queueFamilyIndex);

    CreateOrOverrideSwapChain();

    // descriptor pool cache
    descriptorPoolCache = MakeUnique1<VKDescriptorPoolCache>(context);
    context->descriptorPoolCache = descriptorPoolCache;

    // create main cmdPool
    VkCommandPoolCreateInfo cmdPoolCreateInfo;
    cmdPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolCreateInfo.pNext = VK_NULL_HANDLE;
    cmdPoolCreateInfo.queueFamilyIndex = mainQueue.queueFamilyIndex;
    cmdPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vkCreateCommandPool(device.handle, &cmdPoolCreateInfo, VK_NULL_HANDLE, &mainCmdPool);

    // create inflightData
    inflightData.resize(driverConfig.swapchainImageCount);
    VkCommandBufferAllocateInfo rhiCmdAllocateInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    rhiCmdAllocateInfo.commandPool = mainCmdPool;
    rhiCmdAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    rhiCmdAllocateInfo.commandBufferCount = driverConfig.swapchainImageCount + 1;
    assert(driverConfig.swapchainImageCount <= 8);
    VkCommandBuffer cmds[8];
    vkAllocateCommandBuffers(device.handle, &rhiCmdAllocateInfo, cmds);

    VkFenceCreateInfo rhiFenceCreateInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    rhiFenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // pipeline waits for the cmd to be finished before it
                                                             // records again so we need to it as signaled
    VkSemaphoreCreateInfo semaphoreCreateInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

    inflightData.resize(driverConfig.swapchainImageCount);
    for (int i = 0; i < driverConfig.swapchainImageCount; ++i)
    {
        inflightData[i].cmd = cmds[i];
        inflightData[i].swapchainIndex = i;
        vkCreateFence(device.handle, &rhiFenceCreateInfo, VK_NULL_HANDLE, &inflightData[i].cmdFence);

        vkCreateSemaphore(device.handle, &semaphoreCreateInfo, VK_NULL_HANDLE, &inflightData[i].cmdSemaphore);
        vkCreateSemaphore(device.handle, &semaphoreCreateInfo, VK_NULL_HANDLE, &inflightData[i].presentSemaphore);
    }
    immediateCmd = cmds[driverConfig.swapchainImageCount];
    vkCreateFence(device.handle, &rhiFenceCreateInfo, VK_NULL_HANDLE, &immediateCmdFence);
}

VKDriver::~VKDriver()
{
    vkDeviceWaitIdle(device.handle);

    sharedResource = nullptr;

    descriptorPoolCache = nullptr;
    objectManager->DestroyPendingResources();

    // destroy inflight data
    vkDestroyCommandPool(device.handle, mainCmdPool, VK_NULL_HANDLE);
    for (InflightData& inflight : inflightData)
    {
        vkDestroyFence(device.handle, inflight.cmdFence, VK_NULL_HANDLE);
        vkDestroySemaphore(device.handle, inflight.cmdSemaphore, VK_NULL_HANDLE);
        vkDestroySemaphore(device.handle, inflight.presentSemaphore, VK_NULL_HANDLE);
    }

    SamplerCachePool::DestroyPool();

    vkDestroySwapchainKHR(device.handle, swapchain.handle, VK_NULL_HANDLE);
    memAllocator = nullptr;
    objectManager = nullptr;
    vkDestroySurfaceKHR(instance.handle, surface.handle, VK_NULL_HANDLE);

    // destroy appWindow
    SDL_QuitSubSystem(SDL_INIT_VIDEO);

    vkDestroyDevice(device.handle, nullptr);

    if (instance.debugMessenger != VK_NULL_HANDLE)
    {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT
        )vkGetInstanceProcAddr(instance.handle, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr)
        {
            func(instance.handle, instance.debugMessenger, nullptr);
        }
    }

    vkDestroyInstance(instance.handle, nullptr);
}

Extent2D VKDriver::GetSurfaceSize()
{
    auto extent = surface.surfaceCapabilities.currentExtent;
    return {extent.width, extent.height};
}

void VKDriver::WaitForIdle()
{
    vkDeviceWaitIdle(device.handle);
}

Backend VKDriver::GetGfxBackendType()
{
    return Backend::Vulkan;
}

SDL_Window* VKDriver::GetSDLWindow()
{
    return window.handle;
}

Image* VKDriver::GetSwapChainImage()
{
    return swapchainImage.get();
}

void VKDriver::ForceSyncResources()
{
    return; // TODO: reimplementation needed
}

std::unique_ptr<ShaderProgram> VKDriver::CreateShaderProgram(
    const std::string& name,
    std::shared_ptr<const ShaderConfig> config,
    const std::vector<uint32_t>& vert,
    const std::vector<uint32_t>& frag
)
{
    return std::make_unique<VKShaderProgram>(config, context, name, vert, frag);
}

UniPtr<Semaphore> VKDriver::CreateSemaphore(const Semaphore::CreateInfo& createInfo)
{
    return MakeUnique1<VKSemaphore>(createInfo.signaled);
}

UniPtr<Fence> VKDriver::CreateFence(const Fence::CreateInfo& createInfo)
{
    return MakeUnique1<VKFence>(createInfo);
}

UniPtr<Buffer> VKDriver::CreateBuffer(const Gfx::Buffer::CreateInfo& createInfo)
{
    return MakeUnique1<VKBuffer>(createInfo);
}

UniPtr<RenderPass> VKDriver::CreateRenderPass()
{
    return MakeUnique1<VKRenderPass>();
}

UniPtr<FrameBuffer> VKDriver::CreateFrameBuffer(RefPtr<RenderPass> renderPass)
{
    return MakeUnique1<VKFrameBuffer>(renderPass);
}

UniPtr<Image> VKDriver::CreateImage(const ImageDescription& description, ImageUsageFlags usages)
{
    return MakeUnique1<VKImage>(description, usages);
}
UniPtr<ShaderProgram> VKDriver::CreateShaderProgram(
    const std::string& name,
    std::shared_ptr<const ShaderConfig> config,
    const unsigned char* vert,
    uint32_t vertSize,
    const unsigned char* frag,
    uint32_t fragSize
)
{
    return MakeUnique1<VKShaderProgram>(config, context, name, vert, vertSize, frag, fragSize);
}

void VKDriver::QueueSubmit(
    RefPtr<CommandQueue> queue,
    std::span<Gfx::CommandBuffer*> cmdBufs,
    std::span<RefPtr<Semaphore>> waitSemaphores,
    std::span<Gfx::PipelineStageFlags> waitDstStageMasks,
    std::span<RefPtr<Semaphore>> signalSemaphroes,
    RefPtr<Fence> signalFence
)
{
    std::vector<VkSemaphore> vkWaitSemaphores;
    std::vector<VkSemaphore> vkSignalSemaphores;
    std::vector<VkPipelineStageFlags> vkPipelineStageFlags;
    std::vector<VkCommandBuffer> vkCmdBufs;

    for (auto w : waitSemaphores)
    {
        auto vkWaitSemaphore = static_cast<VKSemaphore*>(w.Get());
        vkWaitSemaphores.push_back(vkWaitSemaphore->GetHandle());
    }

    for (auto s : signalSemaphroes)
    {
        auto vkSignalSemaphore = static_cast<VKSemaphore*>(s.Get());
        vkSignalSemaphores.push_back(vkSignalSemaphore->GetHandle());
    }

    for (auto p : waitDstStageMasks)
    {
        vkPipelineStageFlags.push_back(MapPipelineStage(p));
    }

    for (auto c : cmdBufs)
    {
        vkCmdBufs.push_back(static_cast<VKCommandBuffer*>(c)->GetHandle());
    }

    auto vkqueue = static_cast<VKCommandQueue*>(queue.Get());

    VkSubmitInfo submitInfo;
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = VK_NULL_HANDLE;
    submitInfo.waitSemaphoreCount = vkWaitSemaphores.size();
    submitInfo.pWaitSemaphores = vkWaitSemaphores.data();
    submitInfo.pWaitDstStageMask = vkPipelineStageFlags.data();
    submitInfo.commandBufferCount = vkCmdBufs.size();
    submitInfo.pCommandBuffers = vkCmdBufs.data();
    submitInfo.signalSemaphoreCount = vkSignalSemaphores.size();
    submitInfo.pSignalSemaphores = vkSignalSemaphores.data();

    VkFence fence = signalFence == nullptr ? VK_NULL_HANDLE : static_cast<VKFence*>(signalFence.Get())->GetHandle();

    vkQueueSubmit(mainQueue.handle, 1, &submitInfo, fence);
}

UniPtr<CommandPool> VKDriver::CreateCommandPool(const CommandPool::CreateInfo& createInfo)
{
    return MakeUnique1<VKCommandPool>(createInfo);
}

void VKDriver::WaitForFence(std::vector<RefPtr<Fence>>&& fences, bool waitAll, uint64_t timeout)
{
    std::vector<VkFence> vkFences;
    for (auto f : fences)
    {
        vkFences.push_back(static_cast<VKFence*>(f.Get())->GetHandle());
    }

    vkWaitForFences(device.handle, vkFences.size(), vkFences.data(), waitAll, timeout);
}

bool VKDriver::IsFormatAvaliable(ImageFormat format, ImageUsageFlags usages)
{
    VkImageFormatProperties props;
    if (vkGetPhysicalDeviceImageFormatProperties(
            gpu.handle,
            MapFormat(format),
            VK_IMAGE_TYPE_2D,
            VK_IMAGE_TILING_OPTIMAL,
            MapImageUsage(usages),
            0,
            &props
        ) == VK_SUCCESS)
        return true;
    return false;
}

void VKDriver::GenerateMipmaps(VKImage& image)
{
    VkCommandBuffer cmd = inflightData[currentInflightIndex].cmd;

    VkImageSubresourceRange range;
    range.baseArrayLayer = 0;
    range.layerCount = image.GetDescription().GetLayer();
    range.levelCount = 1;
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    for (uint32_t layer = 0; layer < image.GetDescription().GetLayer(); ++layer)
    {
        for (uint32_t mip = 1; mip < image.GetDescription().mipLevels; ++mip)
        {
            range.baseMipLevel = mip - 1;
            VkImageMemoryBarrier vkBarrier[2];
            vkBarrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            vkBarrier[0].pNext = VK_NULL_HANDLE;
            vkBarrier[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            vkBarrier[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            vkBarrier[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            vkBarrier[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            vkBarrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            vkBarrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            vkBarrier[0].image = image.GetImage();
            vkBarrier[0].subresourceRange = range;

            range.baseMipLevel = mip;
            vkBarrier[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            vkBarrier[1].pNext = VK_NULL_HANDLE;
            vkBarrier[1].srcAccessMask = VK_ACCESS_NONE;
            vkBarrier[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            vkBarrier[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            vkBarrier[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            vkBarrier[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            vkBarrier[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            vkBarrier[1].image = image.GetImage();
            vkBarrier[1].subresourceRange = range;

            if (mip == 1)
            {
                vkBarrier[0].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }

            vkCmdPipelineBarrier(
                cmd,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_DEPENDENCY_BY_REGION_BIT,
                0,
                VK_NULL_HANDLE,
                0,
                VK_NULL_HANDLE,
                2,
                vkBarrier
            );

            int32_t width = image.GetDescription().width;
            int32_t height = image.GetDescription().height;
            VkImageBlit blit;
            blit.srcSubresource = {
                .aspectMask = range.aspectMask,
                .mipLevel = mip - 1,
                .baseArrayLayer = range.baseArrayLayer,
                .layerCount = range.layerCount,
            };
            blit.srcOffsets[0] = {0, 0, 0};
            blit.srcOffsets[1] = {width, height, 1};
            blit.dstSubresource = {
                .aspectMask = range.aspectMask,
                .mipLevel = mip,
                .baseArrayLayer = range.baseArrayLayer,
                .layerCount = range.layerCount,
            };
            blit.dstOffsets[0] = {0, 0, 0};
            blit.dstOffsets[1] = {width / 2, height / 2, 1};

            vkCmdBlitImage(
                cmd,
                image.GetImage(),
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                image.GetImage(),
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &blit,
                VK_FILTER_LINEAR
            );
        }
    }

    VkImageMemoryBarrier vkBarrier[2];
    vkBarrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    vkBarrier[0].pNext = VK_NULL_HANDLE;
    vkBarrier[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkBarrier[0].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    vkBarrier[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    vkBarrier[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkBarrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vkBarrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vkBarrier[0].image = image.GetImage();
    range.baseMipLevel = 0;
    range.levelCount = image.GetDescription().mipLevels - 1;
    vkBarrier[0].subresourceRange = range;

    vkBarrier[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    vkBarrier[1].pNext = VK_NULL_HANDLE;
    vkBarrier[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkBarrier[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkBarrier[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    vkBarrier[1].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkBarrier[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vkBarrier[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vkBarrier[1].image = image.GetImage();
    vkBarrier[1].subresourceRange = range;
    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_DEPENDENCY_BY_REGION_BIT,
        0,
        VK_NULL_HANDLE,
        0,
        VK_NULL_HANDLE,
        2,
        vkBarrier
    );
}

std::unique_ptr<ShaderProgram> VKDriver::CreateComputeShaderProgram(
    const std::string& name, std::shared_ptr<const ShaderConfig> config, const std::vector<uint32_t>& comp
)
{
    return std::make_unique<VKShaderProgram>(config, context, name, comp);
}

std::unique_ptr<ImageView> VKDriver::CreateImageView(const ImageView::CreateInfo& createInfo)
{
    return std::unique_ptr<ImageView>(new VKImageView(createInfo));
}

void VKDriver::ClearResources()
{
    context->objManager->DestroyPendingResources();
    context->allocator->DestroyPendingResources();
}

std::unique_ptr<ShaderResource> VKDriver::CreateShaderResource()
{
    return std::make_unique<VKShaderResource>();
}
void VKDriver::Present()
{
    // acquire next swapchain
    VkSemaphore imageAcquireSemaphore = inflightData[currentInflightIndex].cmdSemaphore;
    vkAcquireNextImageKHR(
        device.handle,
        swapchain.handle,
        -1,
        imageAcquireSemaphore,
        VK_NULL_HANDLE,
        &inflightData[currentInflightIndex].swapchainIndex
    );
    swapchainImage->SetActiveSwapChainImage(inflightData[currentInflightIndex].swapchainIndex);

    vkWaitForFences(device.handle, 1, &inflightData[currentInflightIndex].cmdFence, true, -1);
    vkResetFences(device.handle, 1, &inflightData[currentInflightIndex].cmdFence);

    // prepare for command execution
    inflightData[currentInflightIndex].pendingCommands.clear();

    // record scheduled commands
    auto cmd = inflightData[currentInflightIndex].cmd;

    vkResetCommandBuffer(cmd, 0);
    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);
    RHI_UploadData(cmd);

    VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        0,
        1,
        &barrier,
        0,
        VK_NULL_HANDLE,
        0,
        VK_NULL_HANDLE
    );

    VKCommandBuffer vkCmd(cmd);
    for (auto& f : inflightData[currentInflightIndex].pendingCommands)
    {
        f(vkCmd);
    }
    vkEndCommandBuffer(cmd);

    VkPipelineStageFlags waitFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &imageAcquireSemaphore;
    submitInfo.pWaitDstStageMask = &waitFlags;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &inflightData[currentInflightIndex].presentSemaphore;
    vkQueueSubmit(mainQueue.handle, 1, &submitInfo, inflightData[currentInflightIndex].cmdFence);

    VkSemaphore presentSemaphore = inflightData[currentInflightIndex].presentSemaphore;
    VkSwapchainKHR swapChainHandle = swapchain.handle;
    VkPresentInfoKHR presentInfo = {
        VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        nullptr,
        1,
        &presentSemaphore,
        1,
        &swapChainHandle,
        &inflightData[currentInflightIndex].swapchainIndex,
        nullptr};
    vkQueuePresentKHR(mainQueue.handle, &presentInfo);

    currentInflightIndex = (currentInflightIndex + 1) % inflightData.size();
}

void VKDriver::CreateAppWindow()
{
    if (!SDL_WasInit(SDL_INIT_VIDEO))
        SDL_InitSubSystem(SDL_INIT_VIDEO);

    SDL_DisplayMode displayMode;
    // MacOS return points not pixels
    SDL_GetCurrentDisplayMode(0, &displayMode);

    if (window.size.width > displayMode.w)
        window.size.width = displayMode.w * 0.8;
    if (window.size.height > displayMode.h)
        window.size.height = displayMode.h * 0.8;

    window.handle = SDL_CreateWindow(
        "WeilanGame",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        window.size.width,
        window.size.height,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
    );

    int drawableWidth, drawbaleHeight;
    SDL_GL_GetDrawableSize(window.handle, &drawableWidth, &drawbaleHeight);

    window.size.width = drawableWidth;
    window.size.height = drawbaleHeight;
}

std::vector<const char*> VKDriver::AppWindowGetRequiredExtensions()
{
    unsigned int count;
    if (!SDL_Vulkan_GetInstanceExtensions(window.handle, &count, nullptr))
    {
        SPDLOG_CRITICAL(SDL_GetError());
    }

    std::vector<const char*> names(count);
    if (!SDL_Vulkan_GetInstanceExtensions(window.handle, &count, names.data()))
    {
        SPDLOG_CRITICAL(SDL_GetError());
    }

    return names;
}

bool VKDriver::Instance_CheckAvalibilityOfValidationLayers(const std::vector<const char*>& validationLayers)
{
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
    for (auto& k : availableLayers)
    {
        SPDLOG_INFO(k.layerName);
    }
    for (const char* layerName : validationLayers)
    {
        bool layerFound = false;
        for (const auto& layerProperties : availableLayers)
        {
            if (strcmp(layerName, layerProperties.layerName) == 0)
            {
                layerFound = true;
                break;
            }
        }

        if (!layerFound)
        {
            return false;
        }
    }

    return true;
}

void VKDriver::CreateInstance()
{
    bool enableValidationLayers = true;

    // Create vulkan application info
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Core Engine";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_MAKE_VERSION(1, 1, 193);
    appInfo.pNext = VK_NULL_HANDLE;

    VkInstanceCreateInfo createInfo{};

    // Create vulkan instance info
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledLayerCount = 0;
    createInfo.ppEnabledLayerNames = VK_NULL_HANDLE;
    createInfo.pNext = VK_NULL_HANDLE;
#if __APPLE__
    createInfo.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif
    std::vector<const char*> extensions = AppWindowGetRequiredExtensions();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = VkDebugUtilsMessengerCreateInfoEXT{};
    std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation",
        "VK_LAYER_KHRONOS_synchronization2"}; // If you don't get syncrhonization validation work, be sure it's enabled
                                              // and overrided in vkconfig app in VulkanSDK
    if (enableValidationLayers)
    {
        if (!Instance_CheckAvalibilityOfValidationLayers(validationLayers))
        {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        // VK_EXT_DEBUG_UTILS_EXTENSION_NAME enables message callback
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#if __APPLE__
        extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#endif
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();

        // Enable Debug message
        debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugCreateInfo.messageSeverity =
            // VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugCreateInfo.pfnUserCallback = this->DebugCallback;
        debugCreateInfo.pUserData = nullptr; // Optional

        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
    }

    // Enable instance extension
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    auto result = vkCreateInstance(&createInfo, nullptr, &instance.handle);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create instance!");
    }

    if (enableValidationLayers)
    {
        // If we enable validation layer, then we also want to enable debug messenger
        if (CreateDebugUtilsMessengerEXT(instance.handle, &debugCreateInfo, nullptr, &instance.debugMessenger) !=
            VK_SUCCESS)
        {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }

    VKDebugUtils::Init(instance.handle);
}

VkBool32 VKDriver::DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData
)
{
    switch (messageSeverity)
    {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            {
                SPDLOG_INFO(pCallbackData->pMessage);
                return VK_FALSE;
            }
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            {
                SPDLOG_INFO(pCallbackData->pMessage);
                return VK_FALSE;
            }
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            {
                SPDLOG_WARN(pCallbackData->pMessage);
                return VK_FALSE;
            }
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            {
                SPDLOG_ERROR(pCallbackData->pMessage);
                return VK_FALSE;
            }
        default: break;
    }

    return VK_FALSE;
}

void VKDriver::CreatePhysicalDevice()
{
    // Get all physical devices
    uint32_t count;
    std::vector<VkPhysicalDevice> physicalDevices;

    VkResult result = vkEnumeratePhysicalDevices(instance.handle, &count, nullptr);
    if (result != VK_SUCCESS)
    {
        std::runtime_error("no physical devices enumerated");
    }

    physicalDevices.resize(count);

    result = vkEnumeratePhysicalDevices(instance.handle, &count, physicalDevices.data());
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("can't get physical devices");
    }

    std::vector<GPU> gpus;
    for (auto pd : physicalDevices)
    {
        GPU thisGPU{pd};

        vkGetPhysicalDeviceProperties(thisGPU.handle, &thisGPU.physicalDeviceProperties);
        vkGetPhysicalDeviceFeatures(thisGPU.handle, &thisGPU.physicalDeviceFeatures);
        vkGetPhysicalDeviceMemoryProperties(thisGPU.handle, &thisGPU.memProperties);

        gpus.push_back(thisGPU);
    }

    for (auto& g : gpus)
    {
        // Check required device extensions
        std::set<std::string> requiredExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

        for (const auto& extension : g.availableExtensions)
        {
            requiredExtensions.erase(extension.extensionName);
        }

        if (!requiredExtensions.empty())
            continue; // early skip if this device is not suitable

        // Check required device features
        if (!true) // no required feature yet
            continue;

        // This gpu passed all the test!
        this->gpu = g;
        break;
    }

    throw std::runtime_error("No Suitable GPU");
}

VkResult VKDriver::CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger
)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else
    {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void VKDriver::CreateSurface()
{
    if (!SDL_Vulkan_CreateSurface(window.handle, instance.handle, &surface.handle))
    {
        spdlog::critical("Window surface creation failed: {0}", SDL_GetError());
    }

    // Get surface capabilities
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu.handle, surface.handle, &surface.surfaceCapabilities) !=
        VK_SUCCESS)
    {
        throw std::runtime_error("Could not check presentation surface capabilities!");
    }

    // Get surface present mode
    uint32_t presentModesCount;
    if ((vkGetPhysicalDeviceSurfacePresentModesKHR(gpu.handle, surface.handle, &presentModesCount, nullptr) !=
         VK_SUCCESS) ||
        (presentModesCount == 0))
    {
        throw std::runtime_error("Error occurred during presentation surface present modes enumeration!");
    }

    surface.surfacePresentModes.resize(presentModesCount);
    if (vkGetPhysicalDeviceSurfacePresentModesKHR(
            gpu.handle,
            surface.handle,
            &presentModesCount,
            &surface.surfacePresentModes[0]
        ) != VK_SUCCESS)
    {
        SPDLOG_ERROR("Error occurred during presentation surface present modes enumeration!");
    }

    // Get surface formats
    uint32_t formatsCount;
    if ((vkGetPhysicalDeviceSurfaceFormatsKHR(gpu.handle, surface.handle, &formatsCount, nullptr) != VK_SUCCESS) ||
        (formatsCount == 0))
    {
        throw std::runtime_error("Error occurred during presentation surface formats enumeration!");
    }

    surface.surfaceFormats.resize(formatsCount);
    if (vkGetPhysicalDeviceSurfaceFormatsKHR(gpu.handle, surface.handle, &formatsCount, &surface.surfaceFormats[0]) !=
        VK_SUCCESS)
    {
        throw std::runtime_error("Error occurred during presentation surface formats enumeration!");
    }
}

bool VKDriver::CreateOrOverrideSwapChain()
{
    vkDeviceWaitIdle(device.handle);

    // Get Format
    auto& surfaceFormats = surface.surfaceFormats;

    // Check if list contains most widely used R8 G8 B8 A8 format
    // with nonlinear color space
    bool foundSurfaceFormat = false;
    for (const VkSurfaceFormatKHR& surfaceFormat : surfaceFormats)
    {
        if (surfaceFormat.format == VK_FORMAT_B8G8R8A8_UNORM &&
            surfaceFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            swapchain.surfaceFormat = surfaceFormat;
            foundSurfaceFormat = true;
            break;
        }
    }

    if (!foundSurfaceFormat)
        return false;

    // check surface capabilities
    if ((surface.surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT &
         VK_IMAGE_USAGE_TRANSFER_DST_BIT) ||
        std::find(surface.surfacePresentModes.begin(), surface.surfacePresentModes.end(), VK_PRESENT_MODE_FIFO_KHR) ==
            surface.surfacePresentModes.end())
    {
        throw std::runtime_error("unsupported usage");
    }

    swapchain.extent = surface.surfaceCapabilities.currentExtent;
    swapchain.imageUsageFlags = surface.surfaceCapabilities.supportedUsageFlags;
    swapchain.surfaceTransform = surface.surfaceCapabilities.currentTransform;
    swapchain.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchain.numberOfImages = driverConfig.swapchainImageCount;

    VkSwapchainKHR oldSwapChain = swapchain.handle;

    VkSwapchainCreateInfoKHR swapChainCreateInfo = {
        VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        nullptr,
        0,
        surface.handle,
        swapchain.numberOfImages,
        swapchain.surfaceFormat.format,
        swapchain.surfaceFormat.colorSpace,
        swapchain.extent,
        1,
        swapchain.imageUsageFlags,
        VK_SHARING_MODE_EXCLUSIVE,
        0,
        nullptr,
        swapchain.surfaceTransform,
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        swapchain.presentMode,
        VK_TRUE,
        oldSwapChain};

    if (swapchain.extent.width == 0 || swapchain.extent.height == 0)
    {
        return false;
    }

    if (vkCreateSwapchainKHR(device.handle, &swapChainCreateInfo, nullptr, &swapchain.handle) != VK_SUCCESS)
    {
        std::runtime_error("Cloud not create swap chain!");
    }

    if (oldSwapChain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(device.handle, oldSwapChain, nullptr);
    }

    bool r = Swapchain_GetImagesFromVulkan();
    if (!r)
        return false;

    int minImageCount = surface.surfaceCapabilities.minImageCount;
    int maxImageCount = surface.surfaceCapabilities.maxImageCount;
    driverConfig.swapchainImageCount =
        driverConfig.swapchainImageCount < minImageCount ? minImageCount : driverConfig.swapchainImageCount;
    driverConfig.swapchainImageCount =
        driverConfig.swapchainImageCount > maxImageCount ? maxImageCount : driverConfig.swapchainImageCount;

    return true;
}

bool VKDriver::Swapchain_GetImagesFromVulkan()
{
    swapchainImage = nullptr;
    uint32_t imageCount = 0;

    vkGetSwapchainImagesKHR(device.handle, swapchain.handle, &imageCount, VK_NULL_HANDLE);
    std::vector<VkImage> swapChainImagesTemp(imageCount);
    if (vkGetSwapchainImagesKHR(device.handle, swapchain.handle, &imageCount, swapChainImagesTemp.data()) != VK_SUCCESS)
    {
        return false;
    }

    swapchainImage = std::make_unique<VKSwapChainImage>();
    swapchainImage->Recreate(
        swapChainImagesTemp,
        swapchain.surfaceFormat.format,
        swapchain.extent.width,
        swapchain.extent.height,
        swapchain.imageUsageFlags
    );
    swapchainImage->SetName("Swapchain Image");

    return true;
}

void VKDriver::CreateDevice()
{
    struct QueueRequest
    {
        VkQueueFlags flags;
        bool requireSurfaceSupport;
        float priority;
    };
    const int requestsCount = 1;
    const int mainQueueIndex = 0;
    QueueRequest queueRequests[requestsCount] = {
        {VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT | VK_QUEUE_COMPUTE_BIT, true, 1}};

    uint32_t queueFamilyIndices[16];
    float queuePriorities[16][16];
    auto& queueFamilyProperties = gpu.queueFamilyProperties;
    for (int i = 0; i < requestsCount; ++i)
    {
        QueueRequest request = queueRequests[i];
        int queueFamilyIndex = 0;
        bool found = false;
        for (; queueFamilyIndex < queueFamilyProperties.size(); ++queueFamilyIndex)
        {
            if (queueFamilyProperties[queueFamilyIndex].queueFlags & request.flags)
            {
                VkBool32 surfaceSupport = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(gpu.handle, queueFamilyIndex, surface.handle, &surfaceSupport);
                if (surfaceSupport && request.requireSurfaceSupport)
                {
                    found = true;
                    break;
                }

                if (!request.requireSurfaceSupport)
                {
                    found = true;
                    break;
                }
            }
        }
        if (!found)
            throw std::runtime_error("Vulkan: Can't find required queue family index");

        queueFamilyIndices[i] = queueFamilyIndex;
        queuePriorities[i][0] = request.priority;
    }

    VkDeviceQueueCreateInfo queueCreateInfos[16];

    int queueCreateInfoCount = 0;
    for (int i = 0; i < requestsCount; ++i)
    {
        bool skip = false;
        // found duplicate queueFamilyIndex
        for (int j = 0; j < i; ++j)
        {
            if (queueCreateInfos[j].queueFamilyIndex == queueFamilyIndices[i])
            {
                queueCreateInfos[j].queueCount += 1;
                queuePriorities[j][queueCreateInfos[j].queueCount - 1] = queueRequests[i].priority;
                skip = true;
                break;
            }
        }

        if (!skip)
        {
            queueCreateInfos[i].flags = 0;
            queueCreateInfos[i].pNext = VK_NULL_HANDLE;
            queueCreateInfos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfos[i].queueFamilyIndex = queueFamilyIndices[i];
            queueCreateInfos[i].queueCount = 1;
            queueCreateInfos[i].pQueuePriorities = queuePriorities[i];
            queueCreateInfoCount += 1;
        }
    }

    VkDeviceCreateInfo deviceCreateInfo = {};

#if __APPLE__
    for (auto extension : gpu.GetAvailableExtensions())
    {
        if (std::strcmp(extension.extensionName, "VK_KHR_portability_subset") == 0)
        {
            deviceExtensions.push_back("VK_KHR_portability_subset");
        }
    }
#endif

    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = VK_NULL_HANDLE;
    deviceCreateInfo.queueCreateInfoCount = queueCreateInfoCount;
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos;

    deviceCreateInfo.pEnabledFeatures = VK_NULL_HANDLE;
    std::vector<const char*> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
#if ENGINE_EDITOR
    deviceExtensions.push_back(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);
#endif
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
    deviceCreateInfo.enabledExtensionCount = deviceExtensions.size();
    deviceCreateInfo.enabledLayerCount = 0;
    deviceCreateInfo.ppEnabledLayerNames = VK_NULL_HANDLE;

    vkCreateDevice(gpu.handle, &deviceCreateInfo, VK_NULL_HANDLE, &device.handle);

    // Get the device' queue
    VkQueue queue = VK_NULL_HANDLE;
    uint32_t queueIndex = 0;
    // make sure each queue is unique
    vkGetDeviceQueue(device.handle, queueFamilyIndices[mainQueueIndex], queueIndex, &queue);

    assert(queue != VK_NULL_HANDLE);
    mainQueue.handle = queue;
    mainQueue.queueIndex = queueIndex;
    mainQueue.queueFamilyIndex = queueFamilyIndices[mainQueueIndex];

    // get extension address
    VKExtensionFunc::vkCmdPushDescriptorSetKHR =
        (PFN_vkCmdPushDescriptorSetKHR)vkGetDeviceProcAddr(device.handle, "vkCmdPushDescriptorSetKHR");
    if (!VKExtensionFunc::vkCmdPushDescriptorSetKHR)
    {
        throw std::runtime_error("Could not get a valid function pointer for vkCmdPushDescriptorSetKHR");
    }
}

void VKDriver::RHI_UploadData(VkCommandBuffer cmd)
{
    if (nextBufferUploads.empty())
        return;

    // try releasing in used buffer
    for (auto& inflight : inflightData)
    {
        if (inflight.stagingBuffer != nullptr && inflight.stagingBuffer->inUse)
        {
            if (vkGetFenceStatus(device.handle, inflight.cmdFence) == VK_SUCCESS)
            {
                inflight.stagingBuffer->inUse = false;
                for (auto& p : inflight.pendingBufferUploads)
                {
                    if (p.finishCallback)
                        p.finishCallback();
                }
                inflight.pendingBufferUploads.clear();
            }
        }
    }

    // found a fitting buffer
    Buffer* stagingBuffer = nullptr;
    size_t totalStaingSize = nextBufferUploadsSize + nextImageUploadsSize;
    for (auto& b : stagingBuffers)
    {
        if (!b->inUse)
        {
            if (b->size >= totalStaingSize)
            {
                stagingBuffer = b.get();
                break;
            }
        }
    }

    // no free buffer, create one
    if (stagingBuffer == VK_NULL_HANDLE)
    {
        Buffer bufVal = Driver_CreateBuffer(
            totalStaingSize * 1.2f, // slighting enlarge staging buffer
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
        );

        auto buf = std::make_unique<Buffer>(bufVal);

        stagingBuffer = buf.get();
        stagingBuffers.push_back(std::move(buf));
    }

    // attach this staging buffer to inflightData
    inflightData[currentInflightIndex].stagingBuffer = stagingBuffer;
    stagingBuffer->inUse = true;

    // try destroy buffer that are overly created
    size_t overCreatedCount = stagingBuffers.size() - driverConfig.swapchainImageCount;
    while (overCreatedCount > 0)
    {
        int index = -1;
        size_t minSize = std::numeric_limits<size_t>::max();

        // find a buffer with minimum size
        for (int i = 0; i < stagingBuffers.size(); ++i)
        {
            if (stagingBuffers[i]->size < minSize && !stagingBuffers[i]->inUse)
            {
                index = i;
                minSize = stagingBuffers[i]->size;
            }
        }

        // destroy that buffer
        if (index >= 0)
        {
            Driver_DestroyBuffer(stagingBuffers[index].get());
            stagingBuffers.erase(stagingBuffers.begin() + index);
        }

        overCreatedCount -= 1;
    }

    // upload data
    size_t stagingOffset = 0;
    for (auto& b : nextBufferUploads)
    {
        VkBufferCopy region{

            .srcOffset = stagingOffset,
            .dstOffset = b.dstOffset,
            .size = b.size,
        };
        stagingOffset += b.size;
        vkCmdCopyBuffer(cmd, stagingBuffer->handle, b.dst, 1, &region);
    }

    if (!nextImageUploads.empty())
    {
        std::vector<VkBufferImageCopy> regions;
        regions.reserve(32);
        for (size_t i = 0; i < nextImageUploads.size(); ++i)
        {
            auto b = nextImageUploads[i].dst;
            VkBufferImageCopy region;
            region.bufferOffset = stagingOffset;
            region.bufferRowLength = 0;
            region.bufferImageHeight = 0;
            region.imageSubresource.aspectMask = nextImageUploads[i].aspect;
            region.imageSubresource.mipLevel = nextImageUploads[i].mipLevel;
            region.imageSubresource.baseArrayLayer = nextImageUploads[i].arrayLayer;
            region.imageSubresource.layerCount = 1;
            region.imageOffset = VkOffset3D{0, 0, 0};
            region.imageExtent = VkExtent3D{b->GetDescription().width, b->GetDescription().height, 1};
            regions.push_back(region);

            if (i + 1 == nextImageUploads.size() || nextImageUploads[i + 1].dst != b)
            {
                vkCmdCopyBufferToImage(
                    cmd,
                    stagingBuffer->handle,
                    b->GetImage(),
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    regions.size(),
                    regions.data()
                );

                regions.clear();
            }
        }
    }

    // move all to inflight so that it can notify uploads when cmd finished
    inflightData[currentInflightIndex].pendingBufferUploads = std::move(nextBufferUploads);
    inflightData[currentInflightIndex].pendingImageUploads = std::move(nextImageUploads);
}

VKDriver::Buffer VKDriver::Driver_CreateBuffer(
    size_t size, VkBufferUsageFlags usage, VmaAllocationCreateFlags vmaCreateFlags
)
{
    VKDriver::Buffer buf;
    buf.size = size;
    buf.inUse = false;

    VkBufferCreateInfo vkCreateInfo{};
    vkCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vkCreateInfo.pNext = VK_NULL_HANDLE;
    vkCreateInfo.flags = 0;
    vkCreateInfo.size = size;
    vkCreateInfo.usage = usage;
    vkCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateInfo.queueFamilyIndexCount = 1;
    uint32_t queueFamily = mainQueue.queueFamilyIndex;
    vkCreateInfo.pQueueFamilyIndices = &queueFamily;

    VmaAllocationCreateInfo allocationCreateInfo{};
    allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocationCreateInfo.flags = vmaCreateFlags;

    memAllocator->CreateBuffer(vkCreateInfo, allocationCreateInfo, buf.handle, buf.allocation, &buf.allocationInfo);

    return buf;
}

void VKDriver::Driver_DestroyBuffer(Buffer* b)
{
    memAllocator->DestroyBuffer(b->handle, b->allocation);
}

void VKDriver::Schedule(std::function<void(Gfx::CommandBuffer& cmd)>&& f)
{
    inflightData[currentInflightIndex].pendingCommands.push_back(std::move(f));
}

void VKDriver::ExecuteImmediately(std::function<void(Gfx::CommandBuffer& cmd)>&& f)
{
    vkResetCommandBuffer(immediateCmd, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(immediateCmd, &beginInfo);
    VKCommandBuffer cmd(immediateCmd);
    f(cmd);
    vkEndCommandBuffer(immediateCmd);

    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.waitSemaphoreCount = 0;
    submitInfo.pWaitSemaphores = VK_NULL_HANDLE;
    submitInfo.pWaitDstStageMask = VK_NULL_HANDLE;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &immediateCmd;
    submitInfo.signalSemaphoreCount = 0;
    submitInfo.pSignalSemaphores = VK_NULL_HANDLE;
    vkQueueSubmit(mainQueue.handle, 1, &submitInfo, immediateCmdFence);

    vkWaitForFences(device.handle, 1, &immediateCmdFence, true, -1);
    vkResetFences(device.handle, 1, &immediateCmdFence);
}

void VKDriver::UploadBuffer(
    Gfx::Buffer& dst, uint8_t* data, size_t size, size_t dstOffset, std::function<void()> finishCallback
)
{
    auto& vkDst = static_cast<VKBuffer&>(dst);

    nextBufferUploadsSize += size;
    nextBufferUploads.push_back(PendingBufferUpload{
        .dst = vkDst.GetHandle(),
        .data = data,
        .size = size,
        .dstOffset = dstOffset,
        .finishCallback = finishCallback,
    });
};
void VKDriver::UploadImage(
    Gfx::Image& dst,
    uint8_t* data,
    size_t size,
    uint32_t mipLevel,
    uint32_t arrayLayer,
    Gfx::ImageAspect aspect,
    std::function<void()> finishedCallback
)
{
    auto& vkDst = static_cast<VKImage&>(dst);

    nextImageUploadsSize += size;
    nextImageUploads.push_back(PendingImageUpload{
        .dst = &vkDst,
        .data = data,
        .size = size,
        .mipLevel = mipLevel,
        .arrayLayer = arrayLayer,
        .aspect = Gfx::MapImageAspect(aspect),
        .finishCallback = finishedCallback,
    });
}
} // namespace Gfx
