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
    swapchain = new VKSwapChain(mainQueue->queueFamilyIndex, swapchainImageCount, gpu, *surface);
    context->swapchain = swapchain;
    swapChainImage = swapchain->GetSwapchainImage();

    // descriptor pool cache
    descriptorPoolCache = MakeUnique1<VKDescriptorPoolCache>(context);
    context->descriptorPoolCache = descriptorPoolCache;

    // create main cmdPool
    VkCommandPoolCreateInfo cmdPoolCreateInfo;
    cmdPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolCreateInfo.pNext = VK_NULL_HANDLE;
    cmdPoolCreateInfo.queueFamilyIndex = mainQueue->GetFamilyIndex();
    cmdPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vkCreateCommandPool(device_vk, &cmdPoolCreateInfo, VK_NULL_HANDLE, &mainCmdPool);

    // create inflightData
    inflightData.resize(swapchainImageCount);
    VkCommandBufferAllocateInfo inflightCmdAllocateInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    inflightCmdAllocateInfo.commandPool = mainCmdPool;
    inflightCmdAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    inflightCmdAllocateInfo.commandBufferCount = swapchainImageCount;
    assert(swapchainImageCount <= 8);
    VkCommandBuffer cmds[8];
    vkAllocateCommandBuffers(device_vk, &inflightCmdAllocateInfo, cmds);

    VkFenceCreateInfo inflightFenceCreateInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    inflightFenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // pipeline waits for the cmd to be finished before it
                                                                  // records again so we need to it as signaled

    inflightData.resize(swapchainImageCount);
    for (int i = 0; i < swapchainImageCount; ++i)
    {
        inflightData[i].cmd = cmds[i];
        inflightData[i].swapchainIndex = i;
        vkCreateFence(device_vk, &inflightFenceCreateInfo, VK_NULL_HANDLE, &inflightData[i].cmdFence);
    }
}

VKDriver::~VKDriver()
{
    vkDeviceWaitIdle(device_vk);

    sharedResource = nullptr;

    descriptorPoolCache = nullptr;
    objectManager->DestroyPendingResources();

    vkDestroyCommandPool(device_vk, mainCmdPool, VK_NULL_HANDLE);
    for (InflightData& inflight : inflightData)
    {
        vkDestroyFence(device_vk, inflight.cmdFence, VK_NULL_HANDLE);
    }

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
        case QueueType::Main: return static_cast<CommandQueue*>(mainQueue.Get());
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

void VKDriver::GenerateMipmaps(VKImage& image)
{
    VkCommandBuffer cmd = GetCurrentCmd();

    auto subresourceRange = image.GetSubresourceRange();
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

void VKDriver::Schedule(std::function<void(Gfx::CommandBuffer& cmd)>&& f) {}
void VKDriver::Render() {}
void VKDriver::UploadBuffer(Gfx::Buffer& dst, uint8_t* data, size_t size, size_t dstOffset){};
void VKDriver::UploadImage(Gfx::Image& dst, uint8_t* data, size_t size, uint32_t mipLevel, uint32_t arayLayer){};
} // namespace Gfx
