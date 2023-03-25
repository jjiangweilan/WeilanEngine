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
    graphics0queue = &device->GetQueue(1);
    context->mainQueue = mainQueue;
    gpu = &device->GetGPU();
    device_vk = device->GetHandle();
    objectManager = new VKObjectManager(device_vk);
    context->objManager = objectManager;
    swapChainImageProxy = MakeUnique1<VKSwapChainImageProxy>();

    // Create other objects
    memAllocator = new VKMemAllocator(instance->GetHandle(), device, gpu->GetHandle(), mainQueue->queueFamilyIndex);
    context->allocator = memAllocator;
    sharedResource = MakeUnique1<VKSharedResource>(context);
    context->sharedResource = sharedResource;
    swapchain = new VKSwapChain(mainQueue->queueFamilyIndex, gpu, *surface);
    context->swapchain = swapchain;

    // just set a placeholder image so that others can use the information from the Image before the first frame
    swapChainImageProxy->SetActiveSwapChainImage(swapchain->GetSwapChainImage(0), 0);

    // descriptor pool cache
    descriptorPoolCache = MakeUnique1<VKDescriptorPoolCache>(context);
    context->descriptorPoolCache = descriptorPoolCache;

    VKCommandPool::CreateInfo cmdPoolCreateInfo;
    cmdPoolCreateInfo.queueFamilyIndex = mainQueue->GetFamilyIndex();
    commandPool = MakeUnique<VKCommandPool>(cmdPoolCreateInfo);
    auto cmdBufs = commandPool->AllocateCommandBuffers(CommandBufferType::Primary, 1);
    auto& cmdBuf = cmdBufs[0];
    cmdBuf->Begin();

    GPUBarrier barrier;
    barrier.srcStageMask = Gfx::PipelineStage::All_Commands;
    barrier.dstStageMask = Gfx::PipelineStage::All_Commands;
    barrier.dstAccessMask = Gfx::AccessMask::Memory_Read | Gfx::AccessMask::Memory_Write;
    barrier.srcAccessMask = Gfx::AccessMask::Memory_Read | Gfx::AccessMask::Memory_Write;
    barrier.image = sharedResource->GetDefaultTexture();
    barrier.imageInfo.dstQueueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED;
    barrier.imageInfo.srcQueueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED;
    barrier.imageInfo.oldLayout = Gfx::ImageLayout::Undefined;
    barrier.imageInfo.newLayout = Gfx::ImageLayout::Shader_Read_Only;
    cmdBuf->Barrier(&barrier, 1);

    cmdBuf->End();

    RefPtr<CommandBuffer> cmdBufss[] = {cmdBuf};
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

void VKDriver::WaitForIdle() { vkDeviceWaitIdle(device->GetHandle()); }

Backend VKDriver::GetGfxBackendType() { return Backend::Vulkan; }

SDL_Window* VKDriver::GetSDLWindow() { return appWindow->GetSDLWindow(); }

RefPtr<Image> VKDriver::GetSwapChainImageProxy() { return swapChainImageProxy.Get(); }

void VKDriver::ForceSyncResources()
{
    return; // TODO: reimplementation needed
}

RefPtr<Semaphore> VKDriver::Present(std::vector<RefPtr<Semaphore>>&& semaphores)
{
    std::vector<VkSemaphore> vkSemaphores(semaphores.size());
    for (int i = 0; i < semaphores.size(); ++i)
    {
        vkSemaphores[i] = static_cast<VKSemaphore*>(semaphores[i].Get())->GetHandle();
    }

    uint32_t activeIndex = swapChainImageProxy->GetActiveIndex();
    VkSwapchainKHR swapChainHandle = swapchain->GetHandle();
    VkPresentInfoKHR presentInfo = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
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

bool VKDriver::AcquireNextSwapChainImage(RefPtr<Semaphore> imageAcquireSemaphore)
{
    VKSemaphore* s = static_cast<VKSemaphore*>(imageAcquireSemaphore.Get());
    bool swapchainRecreated = swapchain->AcquireNextImage(swapChainImageProxy, s->GetHandle());
    return swapchainRecreated;
}

UniPtr<Semaphore> VKDriver::CreateSemaphore(const Semaphore::CreateInfo& createInfo)
{
    return MakeUnique1<VKSemaphore>(createInfo.signaled);
}

UniPtr<Fence> VKDriver::CreateFence(const Fence::CreateInfo& createInfo) { return MakeUnique1<VKFence>(createInfo); }

UniPtr<Buffer> VKDriver::CreateBuffer(const Buffer::CreateInfo& createInfo)
{
    return MakeUnique1<VKBuffer>(createInfo);
}

UniPtr<ShaderResource> VKDriver::CreateShaderResource(RefPtr<ShaderProgram> shader, ShaderResourceFrequency frequency)
{
    return MakeUnique1<VKShaderResource>(shader, frequency);
}

UniPtr<RenderPass> VKDriver::CreateRenderPass() { return MakeUnique1<VKRenderPass>(); }

UniPtr<FrameBuffer> VKDriver::CreateFrameBuffer(RefPtr<RenderPass> renderPass)
{
    return MakeUnique1<VKFrameBuffer>(renderPass);
}

UniPtr<Image> VKDriver::CreateImage(const ImageDescription& description, ImageUsageFlags usages)
{
    return MakeUnique1<VKImage>(description, usages);
}
UniPtr<ShaderProgram> VKDriver::CreateShaderProgram(const std::string& name,
                                                    const ShaderConfig* config,
                                                    unsigned char* vert,
                                                    uint32_t vertSize,
                                                    unsigned char* frag,
                                                    uint32_t fragSize)
{
    return MakeUnique1<VKShaderProgram>(config, context, name, vert, vertSize, frag, fragSize);
}

RefPtr<CommandQueue> VKDriver::GetQueue(QueueType type)
{
    switch (type)
    {
        case Engine::QueueType::Main: return static_cast<CommandQueue*>(mainQueue.Get());
        case Engine::QueueType::Graphics0: return static_cast<CommandQueue*>(graphics0queue.Get());
    }
}

void VKDriver::QueueSubmit(RefPtr<CommandQueue> queue,
                           std::span<RefPtr<CommandBuffer>> cmdBufs,
                           std::span<RefPtr<Semaphore>> waitSemaphores,
                           std::span<Gfx::PipelineStageFlags> waitDstStageMasks,
                           std::span<RefPtr<Semaphore>> signalSemaphroes,
                           RefPtr<Fence> signalFence)
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
        vkCmdBufs.push_back(static_cast<VKCommandBuffer*>(c.Get())->GetHandle());
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
} // namespace Engine::Gfx
