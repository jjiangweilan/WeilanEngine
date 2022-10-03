#include "VKDriver.hpp"

#include "Internal/VKInstance.hpp"
#include "Internal/VKDevice.hpp"
#include "Internal/VKPhysicalDevice.hpp"
#include "Internal/VKAppWindow.hpp"
#include "Internal/VKSurface.hpp"
#include "Internal/VKEnumMapper.hpp"
#include "Internal/VKMemAllocator.hpp"
#include "Internal/VKSwapChain.hpp"
#include "Internal/VKObjectManager.hpp"
#include "VKShaderResource.hpp"
#include "VKBuffer.hpp"
#include "VKShaderModule.hpp"
#include "VKContext.hpp"

#include "VKFrameBuffer.hpp"
#include "VKRenderPass.hpp"
#include "VKDescriptorPool.hpp"
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
    template<class T, class ...Args>
    UniPtr<T> MakeUnique1(Args&&... args)
    {
        return UniPtr<T>(new T(std::forward<Args>(args)...));
    }

    VKDriver::VKDriver()
    {
        context = MakeUnique1<VKContext>();
        VKContext::context = context;
        appWindow = new VKAppWindow();
        instance = new VKInstance(appWindow->GetVkRequiredExtensions());
        surface = new VKSurface(*instance, appWindow);
        VKDevice::QueueRequest queueRequest[] = 
        {
            {
                VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT | VK_QUEUE_COMPUTE_BIT,
                true,
                1
            }
        };
        device = new VKDevice(instance, surface, queueRequest, 1);
        context->device = device;
        mainQueue = &device->GetQueue(0);
        context->mainQueue = mainQueue;
        gpu = &device->GetGPU();
        device_vk = device->GetHandle();
        objectManager = new VKObjectManager(device_vk);
        context->objManager = objectManager;
        swapChainImageProxy = MakeUnique1<VKSwapChainImageProxy>();
        // create commands
        VkCommandPoolCreateInfo createInfo;
        createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        createInfo.pNext = VK_NULL_HANDLE;
        createInfo.queueFamilyIndex = mainQueue->queueFamilyIndex;
        createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        vkCreateCommandPool(device->GetHandle(), &createInfo, VK_NULL_HANDLE, &commandPool);
        VkCommandBufferAllocateInfo cmdAllocInfo;
        cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdAllocInfo.pNext = VK_NULL_HANDLE;
        cmdAllocInfo.commandPool = commandPool;
        cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdAllocInfo.commandBufferCount = 1;
        vkAllocateCommandBuffers(device_vk, &cmdAllocInfo, &renderingCmdBuf);

        // Create other objects
        memAllocator = new VKMemAllocator(instance->GetHandle(), device, gpu->GetHandle(), mainQueue->queueFamilyIndex);
        context->allocator = memAllocator;
        sharedResource = MakeUnique1<VKSharedResource>(context);
        context->sharedResource = sharedResource;
        swapchain = new VKSwapChain(mainQueue->queueFamilyIndex, gpu, *surface);
        context->swapchain = swapchain;

        // create inflight data
        VkSemaphoreCreateInfo semaphoreCreateInfo;
        semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphoreCreateInfo.pNext = VK_NULL_HANDLE;
        semaphoreCreateInfo.flags = 0;
        objectManager->CreateSemaphore(semaphoreCreateInfo, inFlightFrame.imageAcquireSemaphore);
        objectManager->CreateSemaphore(semaphoreCreateInfo, inFlightFrame.renderingFinishedSemaphore);
        VkFenceCreateInfo fenceCreateInfo;
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.pNext = VK_NULL_HANDLE;
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        vkCreateFence(device_vk, &fenceCreateInfo, VK_NULL_HANDLE, &inFlightFrame.mainQueueFinishedFence);

        // descriptor pool cache
        descriptorPoolCache = MakeUnique1<VKDescriptorPoolCache>(context);
        context->descriptorPoolCache = descriptorPoolCache;
    }

    VKDriver::~VKDriver()
    {
        vkDeviceWaitIdle(device_vk);

        sharedResource = nullptr;
        vkDestroyCommandPool(device_vk, commandPool, VK_NULL_HANDLE);
        objectManager->DestroySemaphore(inFlightFrame.imageAcquireSemaphore);
        objectManager->DestroySemaphore(inFlightFrame.renderingFinishedSemaphore);
        vkDestroyFence(device_vk, inFlightFrame.mainQueueFinishedFence, VK_NULL_HANDLE);
         
        descriptorPoolCache = nullptr;
        objectManager->DestroyPendingResources();
        memAllocator->DestroyPendingResources();

        delete swapchain;
        delete memAllocator;
        delete objectManager;
        delete surface;
        delete appWindow;
        delete device;
        delete instance;
    }

    Extent2D VKDriver::GetWindowSize()
    {
        return appWindow->GetDefaultWindowSize();
    }

    Backend VKDriver::GetGfxBackendType()
    {
        return Backend::Vulkan;
    }

    SDL_Window* VKDriver::GetSDLWindow()
    {
        return appWindow->GetSDLWindow();
    }

    RefPtr<Image> VKDriver::GetSwapChainImageProxy()
    {
        return swapChainImageProxy.Get();
    }

    void VKDriver::ForceSyncResources()
    {
        return; // TODO: reimplementation needed
        // graphicsQueue->WaitForIdle();

        vkResetCommandPool(device_vk, commandPool, 0);

        VkCommandBufferBeginInfo cmdBeginInfo;
        cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmdBeginInfo.pNext = VK_NULL_HANDLE;
        cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        cmdBeginInfo.pInheritanceInfo = VK_NULL_HANDLE;

        vkBeginCommandBuffer(renderingCmdBuf, &cmdBeginInfo);

        memAllocator->RecordPendingCommands(renderingCmdBuf);

        vkEndCommandBuffer(renderingCmdBuf);

        VkSubmitInfo submitInfo;
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext = VK_NULL_HANDLE;
        submitInfo.waitSemaphoreCount = 0;
        submitInfo.pWaitSemaphores = VK_NULL_HANDLE;
        submitInfo.pWaitDstStageMask = 0;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &renderingCmdBuf;
        submitInfo.signalSemaphoreCount = 0;
        submitInfo.pSignalSemaphores = VK_NULL_HANDLE;
        // vkQueueSubmit(device->GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);

        vkDeviceWaitIdle(device->GetHandle());
    }

    void VKDriver::ExecuteCommandBuffer(UniPtr<CommandBuffer>&& cmdBuf)
    {
        VKCommandBuffer* vkCmdBuf = static_cast<VKCommandBuffer*>(cmdBuf.Get());
        cmdBuf.Release();
        pendingCmdBufs.emplace_back(vkCmdBuf);
    }

    void VKDriver::DispatchGPUWork()
    {
        vkQueueWaitIdle(mainQueue->queue);
        swapchain->AcquireNextImage(swapChainImageProxy, inFlightFrame.imageAcquireSemaphore);

        vkWaitForFences(device_vk, 1, &inFlightFrame.mainQueueFinishedFence, true, -1);
        vkResetFences(device_vk, 1, &inFlightFrame.mainQueueFinishedFence);

        VkCommandBufferBeginInfo cmdBeginInfo;
        cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmdBeginInfo.pNext = VK_NULL_HANDLE;
        cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        cmdBeginInfo.pInheritanceInfo = VK_NULL_HANDLE;

        vkBeginCommandBuffer(renderingCmdBuf, &cmdBeginInfo);
        objectManager->DestroyPendingResources();
        memAllocator->DestroyPendingResources();
        memAllocator->RecordPendingCommands(renderingCmdBuf);
        // this function should appear after memAllocator and objectManager's DestoryXXX and RecordXXX,
        // because from user's perspective, all resources should be already when calling this function

        vkCmdPipelineBarrier(
                renderingCmdBuf, 
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_DEPENDENCY_BY_REGION_BIT,
                0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);

        for(auto& cmdBuf : pendingCmdBufs)
        {
            cmdBuf->RecordToVulkanCmdBuf(renderingCmdBuf);
        }

        VkImageSubresourceRange range;
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.baseMipLevel = 0;
        range.levelCount = 1;
        range.baseArrayLayer = 0;
        range.layerCount = 1;
        swapChainImageProxy->TransformLayoutIfNeeded(renderingCmdBuf,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, range);
        vkEndCommandBuffer(renderingCmdBuf);
        VkPipelineStageFlags waitForPresentState = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submitInfo;
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext = VK_NULL_HANDLE;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &inFlightFrame.imageAcquireSemaphore;
        submitInfo.pWaitDstStageMask = &waitForPresentState;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &renderingCmdBuf;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &inFlightFrame.renderingFinishedSemaphore;
        vkQueueSubmit(mainQueue->queue, 1, &submitInfo, inFlightFrame.mainQueueFinishedFence);

        // present
        // change present image's layout
        uint32_t activeIndex = swapChainImageProxy->GetActiveIndex();
        VkSwapchainKHR swapChainHandle = swapchain->GetHandle();
        VkPresentInfoKHR presentInfo = {
            VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            nullptr,
            1,
            &inFlightFrame.renderingFinishedSemaphore,
            1,
            &swapChainHandle,
            &activeIndex,
            nullptr                                 
        };
        vkQueuePresentKHR(mainQueue->queue, &presentInfo);
    }

    UniPtr<GfxBuffer> VKDriver::CreateBuffer(uint32_t size, BufferUsage usage, bool cpuVisible)
    {
        return MakeUnique1<VKBuffer>(size, usage, cpuVisible);
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

    UniPtr<CommandBuffer> VKDriver::CreateCommandBuffer()
    {
        return MakeUnique1<VKCommandBuffer>();
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

}
