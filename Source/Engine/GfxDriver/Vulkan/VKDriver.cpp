#include "VKDriver.hpp"

#include "Internal/VKAppWindow.hpp"
#include "Internal/VKDevice.hpp"
#include "Internal/VKEnumMapper.hpp"
#include "Internal/VKInstance.hpp"
#include "Internal/VKMemAllocator.hpp"
#include "Internal/VKObjectManager.hpp"
#include "Internal/VKPhysicalDevice.hpp"
#include "Internal/VKSurface.hpp"
#include "Internal/VKSwapChain.hpp"
#include "VKBuffer.hpp"
#include "VKCommandPool.hpp"
#include "VKContext.hpp"
#include "VKFence.hpp"
#include "VKShaderModule.hpp"
#include "VKShaderResource.hpp"

#include "VKDescriptorPool.hpp"
#include "VKFrameBuffer.hpp"
#include "VKImageView.hpp"
#include "VKRenderPass.hpp"
#include "VKSharedResource.hpp"

#include <spdlog/spdlog.h>
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

namespace Engine::Gfx
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
    context = MakeUnique1<VKContext>();
    VKContext::context = context;
    appWindow = new VKAppWindow(createInfo.windowSize);
    instance = new VKInstance(appWindow->GetVkRequiredExtensions());
    surface = new VKSurface(*instance, appWindow);
    VKDevice::QueueRequest queueRequest[] = {
        {VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT | VK_QUEUE_COMPUTE_BIT, true, 1}};
    device = new VKDevice(instance, surface, queueRequest, sizeof(queueRequest) / sizeof(VKDevice::QueueRequest));
    context->device = device;
    mainQueue = &device->GetQueue(0);
    context->mainQueue = mainQueue;
    gpu = &device->GetGPU();
    device_vk = device->GetHandle();
    objectManager = new VKObjectManager(device_vk);
    context->objManager = objectManager;

    // Create other objects
    memAllocator = new VKMemAllocator(instance->GetHandle(), device, gpu->GetHandle(), mainQueue->queueFamilyIndex);
    context->allocator = memAllocator;
    sharedResource = MakeUnique1<VKSharedResource>(context);
    context->sharedResource = sharedResource;
    swapchain = new VKSwapChain(mainQueue->queueFamilyIndex, gpu, *surface);
    context->swapchain = swapchain;
    swapChainImage = swapchain->GetSwapchainImage();

    // descriptor pool cache
    descriptorPoolCache = MakeUnique1<VKDescriptorPoolCache>(context);
    context->descriptorPoolCache = descriptorPoolCache;

    VKCommandPool::CreateInfo cmdPoolCreateInfo;
    cmdPoolCreateInfo.queueFamilyIndex = mainQueue->GetFamilyIndex();
    commandPool = MakeUnique<VKCommandPool>(cmdPoolCreateInfo);
    auto cmdBufs = commandPool->AllocateCommandBuffers(CommandBufferType::Primary, 1);
    auto& cmdBuf = cmdBufs[0];
    cmdBuf->Begin();

    std::vector<GPUBarrier> barriers;
    for (int i = 0; i < swapchain->GetSwapChainInfo().numberOfImages; ++i)
    {
        GPUBarrier barrier{
            .image = swapChainImage->GetImage(i),
            .srcStageMask = Gfx::PipelineStage::All_Commands,
            .dstStageMask = Gfx::PipelineStage::All_Commands,
            .srcAccessMask = Gfx::AccessMask::Memory_Read | Gfx::AccessMask::Memory_Write,
            .dstAccessMask = Gfx::AccessMask::Memory_Read | Gfx::AccessMask::Memory_Write,
            .imageInfo =
                {
                    .srcQueueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED,
                    .oldLayout = Gfx::ImageLayout::Undefined,
                    .newLayout = Gfx::ImageLayout::Present_Src_Khr,
                },
        };
        barriers.push_back(barrier);
    }

    // upload image data (as white)
    Gfx::Buffer::CreateInfo bufCreateInfo;
    size_t byteSize = sizeof(uint8_t) * 16;
    uint8_t pxls[16] = {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};
    bufCreateInfo.size = byteSize;
    bufCreateInfo.usages = Gfx::BufferUsage::Transfer_Src;
    bufCreateInfo.debugName = "mesh staging buffer";
    bufCreateInfo.visibleInCPU = true;
    auto stagingBuffer = CreateBuffer(bufCreateInfo);
    memcpy(stagingBuffer->GetCPUVisibleAddress(), pxls, byteSize);

    GPUBarrier defaultTextureBarrier = {
        .image = sharedResource->GetDefaultTexture2D(),
        .srcStageMask = Gfx::PipelineStage::All_Commands,
        .dstStageMask = Gfx::PipelineStage::All_Commands,
        .srcAccessMask = Gfx::AccessMask::Memory_Read | Gfx::AccessMask::Memory_Write,
        .dstAccessMask = Gfx::AccessMask::Memory_Read | Gfx::AccessMask::Memory_Write,
        .imageInfo =
            {
                .srcQueueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED,
                .oldLayout = Gfx::ImageLayout::Undefined,
                .newLayout = Gfx::ImageLayout::Transfer_Dst,
            },
    };
    cmdBuf->Barrier(&defaultTextureBarrier, 1);

    Gfx::BufferImageCopyRegion copy[] = {{
        .srcOffset = 0,
        .layers =
            {
                .aspectMask = sharedResource->GetDefaultTexture2D()->GetSubresourceRange().aspectMask,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = sharedResource->GetDefaultTexture2D()->GetSubresourceRange().layerCount,
            },
        .offset = {0, 0, 0},
        .extend = {2, 2, 1},
    }};

    cmdBuf->CopyBufferToImage(stagingBuffer, sharedResource->GetDefaultTexture2D(), copy);

    defaultTextureBarrier = {
        .image = sharedResource->GetDefaultTexture2D(),
        .srcStageMask = Gfx::PipelineStage::All_Commands,
        .dstStageMask = Gfx::PipelineStage::All_Commands,
        .srcAccessMask = Gfx::AccessMask::Memory_Read | Gfx::AccessMask::Memory_Write,
        .dstAccessMask = Gfx::AccessMask::Memory_Read | Gfx::AccessMask::Memory_Write,
        .imageInfo =
            {
                .srcQueueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED,
                .oldLayout = Gfx::ImageLayout::Transfer_Dst,
                .newLayout = Gfx::ImageLayout::Shader_Read_Only,
            },
    };

    GPUBarrier defaultTextureCubeBarrier = {
        .image = sharedResource->GetDefaultTextureCube(),
        .srcStageMask = Gfx::PipelineStage::All_Commands,
        .dstStageMask = Gfx::PipelineStage::All_Commands,
        .srcAccessMask = Gfx::AccessMask::Memory_Read | Gfx::AccessMask::Memory_Write,
        .dstAccessMask = Gfx::AccessMask::Memory_Read | Gfx::AccessMask::Memory_Write,
        .imageInfo =
            {
                .srcQueueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED,
                .oldLayout = Gfx::ImageLayout::Undefined,
                .newLayout = Gfx::ImageLayout::Shader_Read_Only,
            },
    };

    barriers.push_back(defaultTextureBarrier);
    barriers.push_back(defaultTextureCubeBarrier);
    cmdBuf->Barrier(barriers.data(), barriers.size());

    cmdBuf->End();

    Gfx::CommandBuffer* cmdBufss[] = {cmdBuf.Get()};
    QueueSubmit(mainQueue, cmdBufss, {}, {}, {}, nullptr);

    WaitForIdle();
}

VKDriver::~VKDriver()
{
    vkDeviceWaitIdle(device_vk);

    sharedResource = nullptr;

    descriptorPoolCache = nullptr;
    objectManager->DestroyPendingResources();
    commandPool = nullptr;
    inFlightFrame.imageAcquireSemaphore = nullptr;
    SamplerCachePool::DestroyPool();

    delete swapchain;
    delete memAllocator;
    delete objectManager;
    delete surface;
    delete appWindow;
    delete device;
    delete instance;
}

Extent2D VKDriver::GetSurfaceSize()
{
    auto extent = surface->GetSurfaceCapabilities().currentExtent;
    return {extent.width, extent.height};
}

void VKDriver::WaitForIdle()
{
    vkDeviceWaitIdle(device->GetHandle());
}

Backend VKDriver::GetGfxBackendType()
{
    return Backend::Vulkan;
}

SDL_Window* VKDriver::GetSDLWindow()
{
    return appWindow->GetSDLWindow();
}

Image* VKDriver::GetSwapChainImage()
{
    return swapChainImage;
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

RefPtr<Semaphore> VKDriver::Present(std::vector<RefPtr<Semaphore>>&& semaphores)
{
    std::vector<VkSemaphore> vkSemaphores(semaphores.size());
    for (int i = 0; i < semaphores.size(); ++i)
    {
        vkSemaphores[i] = static_cast<VKSemaphore*>(semaphores[i].Get())->GetHandle();
    }

    uint32_t activeIndex = swapChainImage->GetActiveIndex();
    VkSwapchainKHR swapChainHandle = swapchain->GetHandle();
    VkPresentInfoKHR presentInfo = {
        VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        nullptr,
        (uint32_t)vkSemaphores.size(),
        vkSemaphores.data(),
        1,
        &swapChainHandle,
        &activeIndex,
        nullptr};
    vkQueuePresentKHR(mainQueue->queue, &presentInfo);

    return inFlightFrame.imageAcquireSemaphore.Get();
}

AcquireNextSwapChainImageResult VKDriver::AcquireNextSwapChainImage(RefPtr<Semaphore> imageAcquireSemaphore)
{
    VKSemaphore* s = static_cast<VKSemaphore*>(imageAcquireSemaphore.Get());
    AcquireNextImageResult result = swapchain->AcquireNextImage(s->GetHandle());

    if (result == AcquireNextImageResult::Recreated)
    {
        VKCommandPool::CreateInfo cmdPoolCreateInfo;
        cmdPoolCreateInfo.queueFamilyIndex = mainQueue->GetFamilyIndex();
        commandPool = MakeUnique<VKCommandPool>(cmdPoolCreateInfo);
        auto cmdBufs = commandPool->AllocateCommandBuffers(CommandBufferType::Primary, 1);
        auto& cmdBuf = cmdBufs[0];
        cmdBuf->Begin();

        std::vector<GPUBarrier> barriers;
        for (int i = 0; i < swapchain->GetSwapChainInfo().numberOfImages; ++i)
        {
            GPUBarrier barrier{
                .image = swapchain->GetSwapchainImage()->GetImage(i),
                .srcStageMask = Gfx::PipelineStage::All_Commands,
                .dstStageMask = Gfx::PipelineStage::All_Commands,
                .srcAccessMask = Gfx::AccessMask::Memory_Read | Gfx::AccessMask::Memory_Write,
                .dstAccessMask = Gfx::AccessMask::Memory_Read | Gfx::AccessMask::Memory_Write,
                .imageInfo =
                    {
                        .srcQueueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED,
                        .dstQueueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED,
                        .oldLayout = Gfx::ImageLayout::Undefined,
                        .newLayout = Gfx::ImageLayout::Present_Src_Khr,
                    },
            };
            barriers.push_back(barrier);
        }
        cmdBuf->Barrier(barriers.data(), barriers.size());
        cmdBuf->End();

        Gfx::CommandBuffer* cmdBufss[] = {cmdBuf.Get()};
        QueueSubmit(mainQueue, cmdBufss, {}, {}, {}, nullptr);

        swapChainImage = swapchain->GetSwapchainImage();

        WaitForIdle();

        return AcquireNextSwapChainImageResult::Recreated;
    }
    else if (result == AcquireNextImageResult::Failed)
        return AcquireNextSwapChainImageResult::Failed;

    return AcquireNextSwapChainImageResult::Succeeded;
}

UniPtr<Semaphore> VKDriver::CreateSemaphore(const Semaphore::CreateInfo& createInfo)
{
    return MakeUnique1<VKSemaphore>(createInfo.signaled);
}

UniPtr<Fence> VKDriver::CreateFence(const Fence::CreateInfo& createInfo)
{
    return MakeUnique1<VKFence>(createInfo);
}

UniPtr<Buffer> VKDriver::CreateBuffer(const Buffer::CreateInfo& createInfo)
{
    return MakeUnique1<VKBuffer>(createInfo);
}

UniPtr<ShaderResource> VKDriver::CreateShaderResource(RefPtr<ShaderProgram> shader, ShaderResourceFrequency frequency)
{
    return MakeUnique1<VKShaderResource>(shader, frequency);
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

RefPtr<CommandQueue> VKDriver::GetQueue(QueueType type)
{
    switch (type)
    {
        case Engine::QueueType::Main: return static_cast<CommandQueue*>(mainQueue.Get());
    }
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

    vkQueueSubmit(vkqueue->queue, 1, &submitInfo, fence);
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

    vkWaitForFences(device_vk, vkFences.size(), vkFences.data(), waitAll, timeout);
}

bool VKDriver::IsFormatAvaliable(ImageFormat format, ImageUsageFlags usages)
{
    VkImageFormatProperties props;
    if (vkGetPhysicalDeviceImageFormatProperties(
            device->GetGPU().GetHandle(),
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
} // namespace Engine::Gfx
