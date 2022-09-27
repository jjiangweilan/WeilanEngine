#include "Code/Ptr.hpp"
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
#include "Exp/VKShaderModule.hpp"

#include "VKFrameBuffer.hpp"
#include "VKRenderPass.hpp"
#include "VKDescriptorPool.hpp"
#include "VKSharedResource.hpp"

#include "VKFactory.hpp"

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
        appWindow = new VKAppWindow();
        instance = new VKInstance(appWindow->GetVkRequiredExtensions());
        surface = new VKSurface(*instance, appWindow);
        device = new VKDevice(instance, surface);
        gpu = &device->GetGPU();
        device_vk = device->GetHandle();

        objectManager = new VKObjectManager(device_vk);

        // create commands
        VkCommandPoolCreateInfo createInfo;
        createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        createInfo.pNext = VK_NULL_HANDLE;
        createInfo.queueFamilyIndex = gpu->GetGraphicsQueueFamilyIndex();
        createInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        vkCreateCommandPool(device->GetHandle(), &createInfo, VK_NULL_HANDLE, &commandPool);
        VkCommandBufferAllocateInfo cmdAllocInfo;
        cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdAllocInfo.pNext = VK_NULL_HANDLE;
        cmdAllocInfo.commandPool = commandPool;
        cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdAllocInfo.commandBufferCount = 1;
        vkAllocateCommandBuffers(device_vk, &cmdAllocInfo, &mainCommandBuffer);

        // Create other objects
        memAllocator = new VKMemAllocator(instance->GetHandle(), device->GetHandle(), gpu->GetHandle(), gpu->GetGraphicsQueueFamilyIndex());
        mainQueue = device->GetGraphicsQueue();
        renderContext = new VKRenderContext();
        context = MakeUnique1<VKContext>();
        context->allocator = memAllocator;
        context->device = device;
        context->objManager = objectManager;
        sharedResource = MakeUnique1<VKSharedResource>(context);
        context->sharedResource = sharedResource;
        swapchain = new VKSwapChain(context.Get(), *gpu, *surface);
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

        // Initialize other modules
        VKDescriptorPoolCache::Init(context);
    }

    VKDriver::~VKDriver()
    {
        vkDeviceWaitIdle(device_vk);

        sharedResource = nullptr;
        vkDestroyCommandPool(device_vk, commandPool, VK_NULL_HANDLE);
        objectManager->DestroySemaphore(inFlightFrame.imageAcquireSemaphore);
        objectManager->DestroySemaphore(inFlightFrame.renderingFinishedSemaphore);
        vkDestroyFence(device_vk, inFlightFrame.mainQueueFinishedFence, VK_NULL_HANDLE);
         
        VKDescriptorPoolCache::Deinit();
        objectManager->DestroyPendingResources();
        memAllocator->DestroyPendingResources();

        delete swapchain;
        delete memAllocator;
        delete objectManager;
        delete renderContext;
        delete surface;
        delete appWindow;
        delete device;
        delete instance;
    }

    Extent2D VKDriver::GetWindowSize()
    {
        return appWindow->GetDefaultWindowSize();
    }

    std::unique_ptr<RenderTarget> VKDriver::CreateRenderTarget(const RenderTargetDescription& renderTargetDescription)
    {
        return std::make_unique<VKRenderTarget>(context.Get(), appWindow, gpu->GetGraphicsQueueFamilyIndex(), renderTargetDescription);
    }

    GfxBackend VKDriver::GetGfxBackendType()
    {
        return GfxBackend::Vulkan;
    }

    SDL_Window* VKDriver::GetSDLWindow()
    {
        return appWindow->GetSDLWindow();
    }

    RenderContext* VKDriver::GetRenderContext()
    {
        return renderContext;
    }

    void VKDriver::SetPresentImage(RefPtr<Image> presentImageSource)
    {
        VKImage **swapchainImage = &inFlightFrame.presentImage;
        presentImageFunc = [=](VkCommandBuffer cmdBuf)
        {   
            VKImage* imageToPresent = static_cast<VKImage*>(presentImageSource.Get());

            VkImageSubresourceLayers layer;
            layer.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            layer.mipLevel = 0;
            layer.baseArrayLayer = 0;
            layer.layerCount = 1;

            VkImageSubresourceRange range;
            range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            range.baseMipLevel = 0;
            range.levelCount = 1;
            range.baseArrayLayer = 0;
            range.layerCount = 1;

            imageToPresent->TransformLayoutIfNeeded(
                cmdBuf,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_ACCESS_TRANSFER_READ_BIT,
                range);
            (*swapchainImage)->TransformLayoutIfNeeded(cmdBuf,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                0, 0, range);

            VkImageBlit imageBlit;
            imageBlit.srcSubresource = layer;
            imageBlit.srcOffsets[0] = {0,0,0};
            imageBlit.srcOffsets[1] = {(int32_t)imageToPresent->GetDescription().width,(int32_t)imageToPresent->GetDescription().height,1};
            imageBlit.dstSubresource = layer;
            imageBlit.dstOffsets[0] = {0,0,0};
            imageBlit.dstOffsets[1] = {(int32_t)swapchain->GetSwapChainInfo().extent.width, (int32_t)swapchain->GetSwapChainInfo().extent.height, 1};
            vkCmdBlitImage(cmdBuf, imageToPresent->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, (*swapchainImage)->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlit, VK_FILTER_NEAREST);
          };
    }

    void VKDriver::ForceSyncResources()
    {
        vkDeviceWaitIdle(device->GetHandle());

        vkResetCommandPool(device_vk, commandPool, 0);

        VkCommandBufferBeginInfo cmdBeginInfo;
        cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmdBeginInfo.pNext = VK_NULL_HANDLE;
        cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        cmdBeginInfo.pInheritanceInfo = VK_NULL_HANDLE;

        vkBeginCommandBuffer(mainCommandBuffer, &cmdBeginInfo);

        memAllocator->RecordPendingCommands(mainCommandBuffer);

        vkEndCommandBuffer(mainCommandBuffer);

        VkSubmitInfo submitInfo;
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext = VK_NULL_HANDLE;
        submitInfo.waitSemaphoreCount = 0;
        submitInfo.pWaitSemaphores = VK_NULL_HANDLE;
        submitInfo.pWaitDstStageMask = 0;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &mainCommandBuffer;
        submitInfo.signalSemaphoreCount = 0;
        submitInfo.pSignalSemaphores = VK_NULL_HANDLE;
        vkQueueSubmit(device->GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);

        vkDeviceWaitIdle(device->GetHandle());
    }

    void VKDriver::DispatchGPUWork()
    {
        vkQueueWaitIdle(mainQueue);
        swapchain->AcquireNextImage(inFlightFrame.imageIndex, inFlightFrame.presentImage, inFlightFrame.imageAcquireSemaphore);

        vkWaitForFences(device_vk, 1, &inFlightFrame.mainQueueFinishedFence, true, -1);
        vkResetFences(device_vk, 1, &inFlightFrame.mainQueueFinishedFence);
        vkResetCommandPool(device_vk, commandPool, 0);

        VkCommandBufferBeginInfo cmdBeginInfo;
        cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmdBeginInfo.pNext = VK_NULL_HANDLE;
        cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        cmdBeginInfo.pInheritanceInfo = VK_NULL_HANDLE;

        vkBeginCommandBuffer(mainCommandBuffer, &cmdBeginInfo);
        objectManager->DestroyPendingResources();
        memAllocator->DestroyPendingResources();
        memAllocator->RecordPendingCommands(mainCommandBuffer);
        // this function should appear after memAllocator and objectManager's DestoryXXX and RecordXXX,
        // because from user's perspective, all resources should be already when calling this function
        context->RecordFramePrepareCommands(mainCommandBuffer);

        vkCmdPipelineBarrier(
                mainCommandBuffer, 
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_DEPENDENCY_BY_REGION_BIT,
                0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
        renderContext->Render(mainCommandBuffer);

        // copy from game output to swapchain image
        if (presentImageFunc) presentImageFunc(mainCommandBuffer);

        // render game ui directly in swapchain image

        renderContext->Render(mainCommandBuffer);

        // change present image's layout
        VkImageSubresourceRange range;
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.baseMipLevel = 0;
        range.levelCount = 1;
        range.baseArrayLayer = 0;
        range.layerCount = 1;
        inFlightFrame.presentImage->TransformLayoutIfNeeded(mainCommandBuffer,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, range);

        vkEndCommandBuffer(mainCommandBuffer);

        VkPipelineStageFlags waitForPresentState = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submitInfo;
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext = VK_NULL_HANDLE;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &inFlightFrame.imageAcquireSemaphore;
        submitInfo.pWaitDstStageMask = &waitForPresentState;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &mainCommandBuffer;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &inFlightFrame.renderingFinishedSemaphore;
        vkQueueSubmit(mainQueue, 1, &submitInfo, inFlightFrame.mainQueueFinishedFence);

        swapchain->PresentImage(mainQueue, inFlightFrame.imageIndex, 1, &inFlightFrame.renderingFinishedSemaphore);
    }

    void VKDriver::InitGfxFactory()
    {
        auto factory = MakeUnique1<VKFactory>();
        factory->context = context;
        GfxFactory::Init(std::move(factory));
    }
}
