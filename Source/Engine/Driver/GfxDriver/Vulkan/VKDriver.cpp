#include "VKDriver.hpp"
#include "Engine/Core/JobSystem.hpp"
#include "Engine/Core/Profiler/Profiler.hpp"
#include "Engine/Library/Assert.hpp"
#include "Internal/VKEnumMapper.hpp"
#include "Internal/VKMemAllocator.hpp"
#include "Internal/VKObjectManager.hpp"
#include "RayTracing/VKRayTracing.hpp"
#include "VKBuffer.hpp"
#include "VKCommandBuffer.hpp"
#include "VKCommandPool.hpp"
#include "VKContext.hpp"
#include "VKDataUploader.hpp"
#include "VKDescriptorPool.hpp"

#include "VKFence.hpp"
#include "VKFrameBuffer.hpp"
#include "VKImageView.hpp"
#include "VKRayTracingContext.hpp"
#include "VKRenderPass.hpp"
#include "VKSampler.hpp"
#include "VKShaderModule.hpp"
#include "VKShaderResource.hpp"
#include "VKSharedResource.hpp"
#include <SDL_syswm.h>
#include <SDL_vulkan.h>

#include <mutex>
#include <set>
#include <spdlog/spdlog.h>
#include <string>

#include "VKCommon.hpp"

#if !_MSC_VER
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnullability-completeness"
#pragma GCC diagnostic ignored "-Wswitch"
#endif
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#if !_MSC_VER
#pragma GCC diagnostic pop
#endif
#if defined(_WIN32) || defined(_WIN64)
#undef CreateSemaphore
#endif

#if NDEBUG
#define CHECK_VK_RESULT(x) x
#else
#define CHECK_VK_RESULT(x) ASSERT((x) == VK_SUCCESS)
#endif

namespace Gfx
{
struct VKDriver::SDLInfo
{
    SDL_SysWMinfo wmInfo;
};
VKDriver::VKDriver(const CreateInfo& createInfo)
{
    featureSettings.enableGPUProfiling = false;
    rayTracingManager = std::make_unique<VKRayTracing::Manager>();

#if ENGINE_DEV_BUILD
    if (createInfo.enableRenderDoc)
        InitializeRenderDoc(createInfo.enableGfxDriverValidation);
#endif

    window = createInfo.window;
    CreateInstance(createInfo.enableGfxDriverValidation);
    CreatePhysicalDevice();
    CreateSurface();
    CreateDevice();

    objectManager = std::make_unique<VKObjectManager>(device.handle);
    memAllocator =
        std::make_unique<VKMemAllocator>(instance.handle, device.handle, gpu.handle, mainQueue.queueFamilyIndex);
    context = std::make_unique<VKContext>();
    VKContext::context = context.get();
    context->driver = this;
    context->allocator = memAllocator.get();
    context->instance = instance.handle;
    context->objManager = objectManager.get();
    context->device = device.handle;
    context->gpu = &gpu;
    context->swapchain = &swapchain;
    context->mainQueue = &mainQueue;

    swapchain.CreateOrOverrideSwapChain(surface, context->driverConfig.swapchainImageCount);

    // descriptor pool cache
    descriptorPoolCache = std::make_unique<VKDescriptorPoolCache>(context);
    context->descriptorPoolCache = descriptorPoolCache.get();

    // create main cmdPool
    VkCommandPoolCreateInfo cmdPoolCreateInfo;
    cmdPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolCreateInfo.pNext = VK_NULL_HANDLE;
    cmdPoolCreateInfo.queueFamilyIndex = mainQueue.queueFamilyIndex;
    cmdPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vkCreateCommandPool(device.handle, &cmdPoolCreateInfo, VK_NULL_HANDLE, &mainCmdPool);
    context->mainCmdPool = mainCmdPool;

    // create inflightData
    frameContexts.resize(context->driverConfig.swapchainImageCount);
    VkCommandBufferAllocateInfo rhiCmdAllocateInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    rhiCmdAllocateInfo.commandPool = mainCmdPool;
    rhiCmdAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    rhiCmdAllocateInfo.commandBufferCount = context->driverConfig.swapchainImageCount + 1;
    ASSERT(context->driverConfig.swapchainImageCount + 1 <= 8);
    VkCommandBuffer cmds[8];
    vkAllocateCommandBuffers(device.handle, &rhiCmdAllocateInfo, cmds);
    for (int i = 0; i < context->driverConfig.swapchainImageCount + 1; i++)
    {
        VKDebugUtils::SetDebugName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)cmds[i], "VKDriver");
    }

    VkFenceCreateInfo rhiFenceCreateInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    rhiFenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // pipeline waits for the cmd to be finished before it
                                                             // records again so we need to it as signaled
    VkSemaphoreCreateInfo semaphoreCreateInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

    int inflightCount = context->driverConfig.swapchainImageCount;
    frameContexts.resize(inflightCount);
    imageAcquireSemaphores.resize(inflightCount);
    presentSemaphores.resize(inflightCount);
    for (int i = 0; i < context->driverConfig.swapchainImageCount; ++i)
    {
        frameContexts[i].cmd = cmds[i];
        frameContexts[i].swapchainIndex = i;
        vkCreateFence(device.handle, &rhiFenceCreateInfo, VK_NULL_HANDLE, &frameContexts[i].cmdFence);
        VKDebugUtils::SetDebugName(VK_OBJECT_TYPE_FENCE, (uint64_t)frameContexts[i].cmdFence, "VKDriver - fence");

        vkCreateSemaphore(device.handle, &semaphoreCreateInfo, VK_NULL_HANDLE, &imageAcquireSemaphores[i]);
        vkCreateSemaphore(device.handle, &semaphoreCreateInfo, VK_NULL_HANDLE, &presentSemaphores[i]);

        VKDebugUtils::SetDebugName(
            VK_OBJECT_TYPE_SEMAPHORE,
            (uint64_t)imageAcquireSemaphores[i],
            ("VKDriver imageAcquireSemaphore " + std::to_string(i)).c_str()
        );

        VKDebugUtils::SetDebugName(
            VK_OBJECT_TYPE_SEMAPHORE,
            (uint64_t)presentSemaphores[i],
            ("VKDriver presentSemaphore" + std::to_string(i)).c_str()
        );

        if (createInfo.gpuTimestampQueryMaxCount != 0 && gpuFeatures.timestampPeriod)
        {
            VkQueryPoolCreateInfo query_pool_info{};
            query_pool_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
            query_pool_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
            query_pool_info.queryCount = static_cast<uint32_t>(createInfo.gpuTimestampQueryMaxCount);
            frameContexts[i].maxtimestapQueryCount = createInfo.gpuTimestampQueryMaxCount;
            vkCreateQueryPool(device.handle, &query_pool_info, nullptr, &frameContexts[i].timestapQueryPool);
        }
    }
    immediateCmd = cmds[context->driverConfig.swapchainImageCount];
    vkCreateFence(device.handle, &rhiFenceCreateInfo, VK_NULL_HANDLE, &immediateCmdFence);
    vkCreateSemaphore(device.handle, &semaphoreCreateInfo, VK_NULL_HANDLE, &transferSignalSemaphore);
    vkCreateSemaphore(device.handle, &semaphoreCreateInfo, VK_NULL_HANDLE, &dataUploaderWaitSemaphore);

    VKDebugUtils::SetDebugName(
        VK_OBJECT_TYPE_SEMAPHORE,
        (uint64_t)transferSignalSemaphore,
        "VKDriver transferSignalSemaphore"
    );

    VKDebugUtils::SetDebugName(
        VK_OBJECT_TYPE_SEMAPHORE,
        (uint64_t)dataUploaderWaitSemaphore,
        "VKDriver dataUploaderWaitSemaphore"
    );

    dataUploader = std::make_unique<VKDataUploader>(this);
    sharedResource = std::make_unique<VKSharedResource>(this);
    context->sharedResource = sharedResource.get();
    commandBufferProcessor = std::make_unique<VKCommandBufferProcessor>(inflightCount, rayTracingManager.get());
    context->resourceAllocator = std::make_unique<VKResourceAllocator>(commandBufferProcessor.get());

    sdlInfo = std::make_unique<SDLInfo>();
    SDL_VERSION(&sdlInfo->wmInfo.version);
    SDL_GetWindowWMInfo(window, &sdlInfo->wmInfo);
}

VKDriver::~VKDriver()
{
    vkDeviceWaitIdle(device.handle);

    rayTracingManager = nullptr;

    context->resourceAllocator = nullptr;
    commandBufferProcessor = nullptr;
    sharedResource = nullptr;

    descriptorPoolCache = nullptr;
    dataUploader = nullptr;
    swapchain.swapchainImage = nullptr;

    // destroy inflight data
    vkDestroyCommandPool(device.handle, mainCmdPool, VK_NULL_HANDLE);
    for (VKFrameContext& inflight : frameContexts)
    {
        vkDestroyFence(device.handle, inflight.cmdFence, VK_NULL_HANDLE);
        vkDestroyQueryPool(device.handle, inflight.timestapQueryPool, VK_NULL_HANDLE);
    }

    for (VkSemaphore semaphore : imageAcquireSemaphores)
    {
        vkDestroySemaphore(device.handle, semaphore, VK_NULL_HANDLE);
    }

    for (VkSemaphore semaphore : presentSemaphores)
    {
        vkDestroySemaphore(device.handle, semaphore, VK_NULL_HANDLE);
    }

    vkDestroySemaphore(device.handle, transferSignalSemaphore, VK_NULL_HANDLE);
    vkDestroySemaphore(device.handle, dataUploaderWaitSemaphore, VK_NULL_HANDLE);
    vkDestroyFence(device.handle, immediateCmdFence, VK_NULL_HANDLE);

    SamplerCachePool::DestroyPool();

    if (swapchain.handle != VK_NULL_HANDLE)
        vkDestroySwapchainKHR(device.handle, swapchain.handle, VK_NULL_HANDLE);
    objectManager = nullptr;
    memAllocator = nullptr;

    if (surface.handle != VK_NULL_HANDLE)
        vkDestroySurfaceKHR(instance.handle, surface.handle, VK_NULL_HANDLE);

    vkDestroyDevice(device.handle, nullptr);

    if (instance.debugMessenger != VK_NULL_HANDLE)
    {
        if (vkDestroyDebugUtilsMessengerEXT != nullptr)
        {
            vkDestroyDebugUtilsMessengerEXT(instance.handle, instance.debugMessenger, nullptr);
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
    return window;
}

Image* VKDriver::GetSwapChainImage()
{
    return swapchain.swapchainImage.get();
}

void VKDriver::ForceSyncResources()
{
    return; // TODO: reimplementation needed
}

std::unique_ptr<ShaderProgram> VKDriver::CreateShaderProgram(PipelineCreateInfo& createInfo)
{
    std::scoped_lock lock(driverMutex);
    return std::make_unique<VKShaderProgram>(context.get(), createInfo);
}

std::unique_ptr<Semaphore> VKDriver::CreateSemaphore(const Semaphore::CreateInfo& createInfo)
{
    std::scoped_lock lock(driverMutex);
    return std::make_unique<VKSemaphore>(createInfo.signaled);
}

std::unique_ptr<Fence> VKDriver::CreateFence(const Fence::CreateInfo& createInfo)
{
    std::scoped_lock lock(driverMutex);
    return std::make_unique<VKFence>(createInfo);
}

std::unique_ptr<Buffer> VKDriver::CreateBuffer(const Gfx::Buffer::CreateInfo& createInfo)
{
    std::scoped_lock lock(driverMutex);
    return std::make_unique<VKBuffer>(createInfo);
}

std::unique_ptr<RenderPass_Deprecated> VKDriver::CreateRenderPass()
{
    std::scoped_lock lock(driverMutex);
    return std::make_unique<VKRenderPass>();
}

std::unique_ptr<FrameBuffer> VKDriver::CreateFrameBuffer(RefPtr<RenderPass_Deprecated> renderPass)
{
    std::scoped_lock lock(driverMutex);
    return std::make_unique<VKFrameBuffer>(renderPass);
}

std::unique_ptr<Image> VKDriver::CreateImage(const ImageDescription& description, ImageUsageFlags usages)
{
    std::scoped_lock lock(driverMutex);
    return std::make_unique<VKImage>(description, usages);
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

    CHECK_VK_RESULT(vkQueueSubmit(mainQueue.handle, 1, &submitInfo, fence));
}

std::unique_ptr<CommandPool> VKDriver::CreateCommandPool(const CommandPool::CreateInfo& createInfo)
{
    return std::make_unique<VKCommandPool>(createInfo);
}

void VKDriver::WaitForFence(std::vector<RefPtr<Fence>>&& fences, bool waitAll, uint64_t timeout)
{
    std::vector<VkFence> vkFences;
    for (auto f : fences)
    {
        vkFences.push_back(static_cast<VKFence*>(f.Get())->GetHandle());
    }

    CHECK_VK_RESULT(vkWaitForFences(device.handle, vkFences.size(), vkFences.data(), waitAll, timeout));
}

void VKDriver::ShaderReloaded()
{
    commandBufferProcessor->ShaderReloaded();
}

bool VKDriver::IsFormatAvaliable(GfxFormat format, ImageUsageFlags usages)
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

void VKDriver::GenerateMipmaps(SRef<VKImage> image)
{
    std::scoped_lock lock(driverMutex);
    internalPendingCommands.push_back(
        [imageRef = image](VkCommandBuffer cmd)
        {
            auto image = imageRef.Get();
            if (image == nullptr)
                return;
            VkImageSubresourceRange range;
            range.baseArrayLayer = 0;
            range.layerCount = image->GetDescription().GetLayer();
            range.levelCount = 1;
            range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            for (uint32_t layer = 0; layer < image->GetDescription().GetLayer(); ++layer)
            {
                for (uint32_t mip = 1; mip < image->GetDescription().mipLevels; ++mip)
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
                    vkBarrier[0].image = image->GetImage();
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
                    vkBarrier[1].image = image->GetImage();
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

                    float scale = glm::pow(0.5f, mip - 1);
                    int32_t width = image->GetDescription().width * scale;
                    int32_t height = image->GetDescription().height * scale;
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
                        image->GetImage(),
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        image->GetImage(),
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
            vkBarrier[0].image = image->GetImage();
            range.baseMipLevel = 0;
            range.levelCount = image->GetDescription().mipLevels - 1;
            vkBarrier[0].subresourceRange = range;

            vkBarrier[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            vkBarrier[1].pNext = VK_NULL_HANDLE;
            vkBarrier[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            vkBarrier[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkBarrier[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            vkBarrier[1].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            vkBarrier[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            vkBarrier[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            vkBarrier[1].image = image->GetImage();
            range.baseMipLevel = image->GetDescription().mipLevels - 1;
            range.levelCount = 1;
            vkBarrier[1].subresourceRange = range;
            vkCmdPipelineBarrier(
                cmd,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                VK_DEPENDENCY_BY_REGION_BIT,
                0,
                VK_NULL_HANDLE,
                0,
                VK_NULL_HANDLE,
                2,
                vkBarrier
            );
        }
    );
}

void VKDriver::InitGfxImage(Gfx::Image& image, glm::vec4 color)
{
    std::scoped_lock lock(driverMutex);
    internalPendingCommands.push_back(
        [imageRef = static_cast<VKImage&>(image).GetSRef<VKImage>(), color](VkCommandBuffer cmd)
        {
            auto* image = imageRef.Get();
            if (image == nullptr)
                return;

            bool isDepth = IsDepthStencilFormat(image->GetDescription().format);
            bool hasStencil = HasStencil(image->GetDescription().format);
            VkImageAspectFlags aspectMask = isDepth
                                                ? (VK_IMAGE_ASPECT_DEPTH_BIT | (hasStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0u))
                                                : VK_IMAGE_ASPECT_COLOR_BIT;

            VkImageSubresourceRange range{
                .aspectMask = aspectMask,
                .baseMipLevel = 0,
                .levelCount = VK_REMAINING_MIP_LEVELS,
                .baseArrayLayer = 0,
                .layerCount = VK_REMAINING_ARRAY_LAYERS,
            };

            // Transition UNDEFINED -> TRANSFER_DST_OPTIMAL
            VkImageMemoryBarrier toTransferDst{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            toTransferDst.srcAccessMask = VK_ACCESS_NONE;
            toTransferDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toTransferDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            toTransferDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toTransferDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toTransferDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toTransferDst.image = image->GetImage();
            toTransferDst.subresourceRange = range;
            vkCmdPipelineBarrier(
                cmd,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_DEPENDENCY_BY_REGION_BIT,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &toTransferDst
            );

            if (isDepth)
            {
                VkClearDepthStencilValue clearValue{.depth = color.r, .stencil = 0};
                vkCmdClearDepthStencilImage(
                    cmd,
                    image->GetImage(),
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    &clearValue,
                    1,
                    &range
                );
            }
            else
            {
                VkClearColorValue clearValue{.float32 = {color.r, color.g, color.b, color.a}};
                vkCmdClearColorImage(
                    cmd,
                    image->GetImage(),
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    &clearValue,
                    1,
                    &range
                );
            }

            // Transition TRANSFER_DST_OPTIMAL -> read-optimal layout
            VkImageLayout finalLayout = isDepth
                                            ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                                            : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkImageMemoryBarrier toShaderRead{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            toShaderRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toShaderRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            toShaderRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toShaderRead.newLayout = finalLayout;
            toShaderRead.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toShaderRead.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toShaderRead.image = image->GetImage();
            toShaderRead.subresourceRange = range;
            vkCmdPipelineBarrier(
                cmd,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_DEPENDENCY_BY_REGION_BIT,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &toShaderRead
            );

            VkImageSubresourceRange trackRange{
                .aspectMask = aspectMask,
                .baseMipLevel = 0,
                .levelCount = image->GetDescription().mipLevels,
                .baseArrayLayer = 0,
                .layerCount = image->GetDescription().GetLayer(),
            };
            image->SetLayout(trackRange, finalLayout, VK_PIPELINE_STAGE_TRANSFER_BIT, toShaderRead.srcAccessMask, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, toShaderRead.dstAccessMask);
        }
    );
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
    std::scoped_lock lock(driverMutex);
    return std::make_unique<VKShaderResource>();
}

std::unique_ptr<Sampler> VKDriver::CreateSampler(const Sampler::CreateInfo& createInfo)
{
    std::scoped_lock lock(driverMutex);
    return std::make_unique<VKSampler>(createInfo);
}

bool VKDriver::BeginFrame()
{
    BeginFrameCapture();

    ENGINE_SCOPED_PROFILE("VKDriver - BeginFrame");

    VKContext::Instance()->currentFrameContext = &frameContexts[currentInflightIndex];
    VKContext::Instance()->currentInflightIndex = currentInflightIndex;
    frameCount++;
    memAllocator->NewFrame(frameCount - 1);

    ENGINE_BEGIN_PROFILE("VKDriver - Wait for fences");
    WaitForCurrentInflightCmd();

    auto cmd = frameContexts[currentInflightIndex].cmd;
    CHECK_VK_RESULT(vkResetCommandBuffer(cmd, 0));

    vkResetFences(device.handle, 1, &frameContexts[currentInflightIndex].cmdFence);
    ENGINE_END_PROFILE

    frameContexts[currentInflightIndex].frameIndex = frameCount - 1;

    // acquire next swapchain
    //
    if (needPresent)
    {
        ENGINE_BEGIN_PROFILE("VKDriver - Acquire Next Image");
        VkResult acquireResult = vkAcquireNextImageKHR(
            device.handle,
            swapchain.handle,
            -1,
            imageAcquireSemaphores[currentInflightIndex],
            VK_NULL_HANDLE,
            &frameContexts[currentInflightIndex].swapchainIndex
        );
        swapchain.swapchainImage->SetActiveSwapChainImage(frameContexts[currentInflightIndex].swapchainIndex);
        ENGINE_END_PROFILE

        for (auto& w : extraWindows)
        {
            VkResult acquireResult = vkAcquireNextImageKHR(
                device.handle,
                w->swapchain.handle,
                -1,
                w->imageAcquireSemaphores[w->activeIndex],
                VK_NULL_HANDLE,
                &w->swapchainIndex
            );
            w->swapchain.swapchainImage->SetActiveSwapChainImage(w->swapchainIndex);
        }
    }

    return true;
}

void VKDriver::FlushPendingCommands()
{
    vkWaitForFences(device.handle, 1, &frameContexts[currentInflightIndex].cmdFence, true, -1);
    vkResetFences(device.handle, 1, &frameContexts[currentInflightIndex].cmdFence);

    dataUploader->UploadAllPending(
        transferSignalSemaphore,
        firstFrame ? VK_NULL_HANDLE : dataUploaderWaitSemaphore,
        VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT
    );

    // record scheduled commands
    auto cmd = frameContexts[currentInflightIndex].cmd;

    vkResetCommandBuffer(cmd, 0);
    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkMemoryBarrier dataUploadBarrier{
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .pNext = VK_NULL_HANDLE,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
    };
    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
        VK_DEPENDENCY_BY_REGION_BIT,
        1,
        &dataUploadBarrier,
        0,
        VK_NULL_HANDLE,
        0,
        VK_NULL_HANDLE
    );

    for (auto& f : internalPendingCommands)
    {
        f(cmd);
    }

    CmdBufExecutionReport report{};
    commandBufferProcessor->Execute(
        framePrepareData,
        frameContexts[currentInflightIndex],
        currentInflightIndex,
        mainQueue,
        featureSettings,
        report
    );

    vkEndCommandBuffer(cmd);

    VkPipelineStageFlags* waitFlags = allocator.Allocate<VkPipelineStageFlags>(1);
    VkSemaphore* waitSemaphores = allocator.Allocate<VkSemaphore>(1);
    VkSemaphore* signalSemaphores = allocator.Allocate<VkSemaphore>(1);
    waitFlags[0] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    waitSemaphores[0] = transferSignalSemaphore;
    signalSemaphores[0] = dataUploaderWaitSemaphore;
    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitFlags;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;
    CHECK_VK_RESULT(vkQueueSubmit(mainQueue.handle, 1, &submitInfo, frameContexts[currentInflightIndex].cmdFence));

    allocator.Reset();
    internalPendingCommands.clear();
}

bool VKDriver::EndFrame()
{
    ENGINE_SCOPED_PROFILE("VKDriver - EndFrame");
    ENGINE_BEGIN_PROFILE("VKDriver - Lock");
    std::scoped_lock lock(driverMutex);
    ENGINE_END_PROFILE

    dataUploader->UploadAllPending(
        transferSignalSemaphore,
        firstFrame ? VK_NULL_HANDLE : dataUploaderWaitSemaphore,
        VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT
    );
    firstFrame = false;

    auto cmd = frameContexts[currentInflightIndex].cmd;

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    CHECK_VK_RESULT(vkBeginCommandBuffer(cmd, &beginInfo));

    ENGINE_BEGIN_PROFILE("Render Graph Execution")
    for (auto& f : internalPendingCommands)
    {
        f(cmd);
    }

    CmdBufExecutionReport execReport{};

    // this section adds present image layout transition to the end of cmd
    if (needPresent)
    {
        VKCommandBuffer cmd2(commandBufferProcessor.get());
        int idx = frameContexts[currentInflightIndex].swapchainIndex;
        cmd2.PresentImage(swapchain.swapchainImage->GetImage(frameContexts[currentInflightIndex].swapchainIndex));
        for (auto& w : extraWindows)
        {
            if (w->presentRequest.requested)
            {
                cmd2.PresentImage(w->swapchain.swapchainImage->GetImage(w->swapchain.swapchainImage->GetActiveIndex()));
            }
        }
        framePrepareData.AppendVKCommandBuffer(&cmd2);
    }

    VKFramePrepareData raytracingFramePrepareData;
    raytracingFramePrepareData.AppendVKCommandBuffer(rayTracingManager->cmdBuffer.get());
    rayTracingManager->cmdBuffer->Reset(true);

    commandBufferProcessor->Execute(
        raytracingFramePrepareData,
        frameContexts[currentInflightIndex],
        currentInflightIndex,
        mainQueue,
        featureSettings,
        execReport
    );

    commandBufferProcessor->Execute(
        framePrepareData,
        frameContexts[currentInflightIndex],
        currentInflightIndex,
        mainQueue,
        featureSettings,
        execReport
    );
    ENGINE_END_PROFILE // Render Graph Execution

        ENGINE_BEGIN_PROFILE("Vulkan End Command Buffer");
    CHECK_VK_RESULT(vkEndCommandBuffer(cmd));
    ENGINE_END_PROFILE // Vulkan End Command Buffer

        int signalSemaphoreCount = 0;
    int waitSemaphoreCount = 0;
    VkPipelineStageFlags* waitFlags = allocator.Allocate<VkPipelineStageFlags>(2 + extraWindows.size());
    VkSemaphore* waitSemaphores = allocator.Allocate<VkSemaphore>(2 + extraWindows.size());
    VkSemaphore* signalSemaphores = allocator.Allocate<VkSemaphore>(2 + extraWindows.size());
    waitFlags[0] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    waitSemaphores[0] = transferSignalSemaphore;
    if (needPresent)
    {
        waitSemaphores[1] = imageAcquireSemaphores[currentInflightIndex];
        waitFlags[1] = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

        signalSemaphores[1] = presentSemaphores[frameContexts[currentInflightIndex].swapchainIndex];
        signalSemaphoreCount++;
        waitSemaphoreCount++;
    }
    signalSemaphores[0] = dataUploaderWaitSemaphore;
    signalSemaphoreCount++;
    signalSemaphoreCount += extraWindows.size();
    waitSemaphoreCount++;
    waitSemaphores += extraWindows.size();

    for (int i = 0; i < extraWindows.size(); ++i)
    {
        waitFlags[i + 2] = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        waitSemaphores[i + 2] = extraWindows[i]->imageAcquireSemaphores[extraWindows[i]->activeIndex];
        signalSemaphores[i + 2] = extraWindows[i]->presentSemaphores[extraWindows[i]->activeIndex];
    }
    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.waitSemaphoreCount = waitSemaphoreCount;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitFlags;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    submitInfo.signalSemaphoreCount = signalSemaphoreCount;
    submitInfo.pSignalSemaphores = signalSemaphores;

    ENGINE_BEGIN_PROFILE("VKDriver - submit")
    auto result = vkQueueSubmit(mainQueue.handle, 1, &submitInfo, frameContexts[currentInflightIndex].cmdFence);
    ENGINE_END_PROFILE

    ENGINE_BEGIN_PROFILE("VKDriver - Query GPU Timestamp");
    if (featureSettings.enableGPUProfiling)
    {
        QueryGPUTimestamp(execReport);
    }
    ENGINE_END_PROFILE

    allocator.Reset();
    bool swapchainRecreated = false;
    if (needPresent)
    {
        ENGINE_BEGIN_PROFILE("VKDriver - present");
        swapchainRecreated = Present(
            presentSemaphores[frameContexts[currentInflightIndex].swapchainIndex],
            swapchain.handle,
            surface,
            swapchain,
            frameContexts[currentInflightIndex].swapchainIndex
        );

        for (auto& w : extraWindows)
        {
            if (w->presentRequest.requested)
            {
                Present(
                    w->presentSemaphores[w->activeIndex],
                    w->swapchain.handle,
                    w->surface,
                    w->swapchain,
                    w->swapchain.swapchainImage->GetActiveIndex()
                );
                w->presentRequest.requested = false;
            }
        }
        ENGINE_END_PROFILE
    }

    ENGINE_BEGIN_PROFILE("VKDriver - Frame End Clear");
    FrameEndClear();
    ENGINE_END_PROFILE

#if __WIN32__
    if (captureFrameBegin && IsRenderDocInitialized())
    {
        renderDocAPI->EndFrameCapture(
            RENDERDOC_DEVICEPOINTER_FROM_VKINSTANCE(instance.handle),
            sdlInfo->wmInfo.info.win.window
        );
        captureFrameBegin = false;
        captureFrame = false;
    }
#endif
    return swapchainRecreated;
}

bool VKDriver::Present(
    VkSemaphore presentSemaphore,
    VkSwapchainKHR swapChainHandle,
    Surface& surface,
    Swapchain& swapchain,
    uint32_t swapchainIndex
)
{
    VkPresentInfoKHR presentInfo = {
        VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        nullptr,
        1,
        &presentSemaphore,
        1,
        &swapChainHandle,
        &swapchainIndex,
        nullptr
    };

    VkResult result = vkQueuePresentKHR(mainQueue.handle, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        surface.QuerySurfaceProperties(gpu.handle);
        swapchain.CreateOrOverrideSwapChain(surface, context->driverConfig.swapchainImageCount);
        return true;
    }

    return false;
}

std::vector<const char*> VKDriver::AppWindowGetRequiredExtensions()
{
    unsigned int count;
    if (!SDL_Vulkan_GetInstanceExtensions(window, &count, nullptr))
    {
        SPDLOG_CRITICAL(SDL_GetError());
    }

    std::vector<const char*> names(count);
    if (!SDL_Vulkan_GetInstanceExtensions(window, &count, names.data()))
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
    // for (auto& k : availableLayers)
    // {
    //     SPDLOG_INFO(k.layerName);
    // }
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

void VKDriver::CreateInstance(bool enableValidationLayers)
{
    if (volkInitialize() != VK_SUCCESS)
    {
        throw std::runtime_error("failed to initialize volk!");
    }

    // Create vulkan application info
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Core Engine";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_MAKE_VERSION(1, 3, 230);
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
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#if __APPLE__
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#endif

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = VkDebugUtilsMessengerCreateInfoEXT{};
    std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation",
        "VK_LAYER_KHRONOS_synchronization2"
    }; // If you don't get syncrhonization validation work, be sure it's enabled
       // and overrided in vkconfig app in VulkanSDK
    if (enableValidationLayers)
    {
        if (!Instance_CheckAvalibilityOfValidationLayers(validationLayers))
        {
            throw std::runtime_error("validation layers requested, but not available!");
        }

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

    volkLoadInstance(instance.handle);

    instance.debugMessenger = nullptr;
    if (enableValidationLayers)
    {
        // If we enable validation layer, then we also want to enable debug messenger
        if (CreateDebugUtilsMessengerEXT(instance.handle, &debugCreateInfo, nullptr, &instance.debugMessenger) !=
            VK_SUCCESS)
        {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }
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

        thisGPU.physicalDeviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;

        thisGPU.physicalDeviceProperties2.pNext = &thisGPU.asProps;
        vkGetPhysicalDeviceProperties(thisGPU.handle, &thisGPU.physicalDeviceProperties);
        vkGetPhysicalDeviceProperties2(thisGPU.handle, &thisGPU.physicalDeviceProperties2);
        vkGetPhysicalDeviceFeatures(thisGPU.handle, &thisGPU.physicalDeviceFeatures);
        vkGetPhysicalDeviceMemoryProperties(thisGPU.handle, &thisGPU.memProperties);

        uint32_t count;
        vkGetPhysicalDeviceQueueFamilyProperties(thisGPU.handle, &count, VK_NULL_HANDLE);
        thisGPU.queueFamilyProperties.resize(count);
        vkGetPhysicalDeviceQueueFamilyProperties(thisGPU.handle, &count, thisGPU.queueFamilyProperties.data());

        vkEnumerateDeviceExtensionProperties(thisGPU.handle, nullptr, &count, nullptr);

        thisGPU.availableExtensions.resize(count);
        vkEnumerateDeviceExtensionProperties(thisGPU.handle, nullptr, &count, thisGPU.availableExtensions.data());

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

        // This gpu passed all the tests!
        this->gpu = g;

        // fill GPU features
        gpuFeatures.timestampPeriod = gpu.physicalDeviceProperties.limits.timestampPeriod;
        gpuFeatures.multiDrawIndirect = gpu.physicalDeviceFeatures.multiDrawIndirect;
        return;
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
    if (vkCreateDebugUtilsMessengerEXT != nullptr)
    {
        return vkCreateDebugUtilsMessengerEXT(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else
    {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void VKDriver::CreateSurface()
{
    if (!SDL_Vulkan_CreateSurface(window, instance.handle, &surface.handle))
    {
        spdlog::critical("Window surface creation failed: {0}", SDL_GetError());
    }

    surface.QuerySurfaceProperties(gpu.handle);
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
        {VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT | VK_QUEUE_COMPUTE_BIT, true, 1}
    };

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

    // #if __APPLE__
    //     for (auto extension : gpu.GetAvailableExtensions())
    //     {
    //         if (std::strcmp(extension.extensionName, "VK_KHR_portability_subset") == 0)
    //         {
    //             deviceExtensions.push_back("VK_KHR_portability_subset");
    //         }
    //     }
    // #endif

    VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures{};
    rayQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
    rayQueryFeatures.rayQuery = true;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures{};
    asFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    asFeatures.pNext = &rayQueryFeatures;
    asFeatures.accelerationStructure = true;

    VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures{};
    bufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
    bufferDeviceAddressFeatures.pNext = &asFeatures;
    bufferDeviceAddressFeatures.bufferDeviceAddress = true;

    VkPhysicalDeviceSynchronization2Features synchronization2Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
        .pNext = &bufferDeviceAddressFeatures,
        .synchronization2 = true,
    };

    VkPhysicalDeviceShaderDrawParametersFeatures shaderDrawParametersFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES,
        .pNext = &synchronization2Features,
        .shaderDrawParameters = true
    };

    // # Enable bindless features
    VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures{};
    descriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
    descriptorIndexingFeatures.pNext = &shaderDrawParametersFeatures;
    descriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing = true;
    descriptorIndexingFeatures.shaderStorageBufferArrayNonUniformIndexing = true;
    descriptorIndexingFeatures.shaderStorageImageArrayNonUniformIndexing = true;
    descriptorIndexingFeatures.descriptorBindingSampledImageUpdateAfterBind = true;
    descriptorIndexingFeatures.descriptorBindingStorageImageUpdateAfterBind = true;
    descriptorIndexingFeatures.descriptorBindingStorageBufferUpdateAfterBind = true;
    descriptorIndexingFeatures.descriptorBindingUpdateUnusedWhilePending = false; // since we are creating per inflight descriptor set, we won't update descriptor set that is in use by GPU, so this feature is not necessary
    descriptorIndexingFeatures.descriptorBindingPartiallyBound = true;
    descriptorIndexingFeatures.descriptorBindingVariableDescriptorCount = true;
    descriptorIndexingFeatures.runtimeDescriptorArray = true;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures{};
    rayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
    rayTracingPipelineFeatures.pNext = &descriptorIndexingFeatures;

    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = &rayTracingPipelineFeatures;
    deviceCreateInfo.queueCreateInfoCount = queueCreateInfoCount;
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos;

    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
    std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_RAY_QUERY_EXTENSION_NAME,
        VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,

    };
#if ENGINE_EDITOR
    deviceExtensions.push_back(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);
#endif

#if __APPLE__
    deviceExtensions.push_back("VK_KHR_portability_subset");
#endif
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
    deviceCreateInfo.enabledExtensionCount = deviceExtensions.size();
    deviceCreateInfo.enabledLayerCount = 0;
    deviceCreateInfo.ppEnabledLayerNames = VK_NULL_HANDLE;

    VkResult createDeviceResult = vkCreateDevice(gpu.handle, &deviceCreateInfo, VK_NULL_HANDLE, &device.handle);
    if (createDeviceResult != VK_SUCCESS)
    {
        spdlog::error("Failed to create Vulkan device, VkResult: {}", (int)createDeviceResult);
        throw std::runtime_error("failed to create device!");
    }

    volkLoadDevice(device.handle);

    // Get the device' queue
    VkQueue queue = VK_NULL_HANDLE;
    uint32_t queueIndex = 0;
    // make sure each queue is unique
    vkGetDeviceQueue(device.handle, queueFamilyIndices[mainQueueIndex], queueIndex, &queue);

    ASSERT(queue != VK_NULL_HANDLE);
    mainQueue.handle = queue;
    mainQueue.queueIndex = queueIndex;
    mainQueue.queueFamilyIndex = queueFamilyIndices[mainQueueIndex];
    if (gpu.physicalDeviceProperties.limits.timestampComputeAndGraphics)
    {
        mainQueue.supportTimestamp = true;
    }
    else
    {
        mainQueue.supportTimestamp = queueFamilyProperties[mainQueue.queueFamilyIndex].timestampValidBits;
    }
}

VKRawBuffer VKDriver::Driver_CreateBuffer(
    size_t size, VkBufferUsageFlags usage, VmaAllocationCreateFlags vmaCreateFlags
)
{
    VKRawBuffer buf;
    buf.size = size;

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

void VKDriver::Driver_DestroyBuffer(VKRawBuffer& b)
{
    memAllocator->DestroyBuffer(b.handle, b.allocation);
}

void VKDriver::FrameEndClear()
{
    for (auto& w : extraWindows)
    {
        w->activeIndex = (w->activeIndex + 1) % w->swapchainCount;
    }
    currentInflightIndex = (currentInflightIndex + 1) % frameContexts.size();
    internalPendingCommands.clear();
    descriptorPoolCache->AppendAndClearCurrentFrameFreeSets();
    ClearResources();
    framePrepareData.Clear();
}

void VKDriver::UploadBuffer(const Gfx::Buffer& dst, uint8_t* data, size_t size, size_t dstOffset)
{
    std::scoped_lock lock(driverMutex);

    auto& vkDst = static_cast<const VKBuffer&>(dst);
    if (dstOffset + size > dst.GetSize())
    {
        SPDLOG_ERROR("Driver: Upload Buffer failed: dstOffset + size > dst.size");
        return;
    }

    if (std::this_thread::get_id() != JobSystem::Instance().GetMainThreadID())
    {
        dataUploader->CacheUploadBuffer(&vkDst, data, size, dstOffset);
    }
    else
    {
        dataUploader->UploadBuffer(&vkDst, data, size, dstOffset);
    }
};
void VKDriver::UploadImage(
    Gfx::Image& dst, uint8_t* data, size_t size, uint32_t mipLevel, uint32_t arrayLayer, Gfx::ImageAspect aspect

)
{
    this->UploadImage(dst, data, size, mipLevel, arrayLayer, aspect, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void VKDriver::UploadImage(
    Gfx::Image& dst,
    uint8_t* data,
    size_t size,
    uint32_t mipLevel,
    uint32_t arrayLayer,
    Gfx::ImageAspect aspect,
    VkImageLayout finalLayout
)
{
    std::scoped_lock lock(driverMutex);
    auto& vkDst = static_cast<VKImage&>(dst);

    if (std::this_thread::get_id() != JobSystem::Instance().GetMainThreadID())
    {
        dataUploader->CacheUploadImage(&vkDst, data, size, mipLevel, arrayLayer, Gfx::MapImageAspect(aspect), finalLayout);
    }
    else
    {
        dataUploader->UploadImage(&vkDst, data, size, mipLevel, arrayLayer, Gfx::MapImageAspect(aspect), finalLayout);
    }
}

void VKDriver::ExecuteCommandBuffer(Gfx::CommandBuffer& cmd)
{
    framePrepareData.AppendVKCommandBuffer(static_cast<VKCommandBuffer*>(&cmd));
}

void VKDriver::ExecuteCommandBufferImmediately(Gfx::CommandBuffer& cmd)
{
    WaitForCurrentInflightCmd();

    dataUploader->UploadAllPending(
        transferSignalSemaphore,
        VK_NULL_HANDLE, // protects by fences
        VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT
    );

    VKCommandBufferProcessor rg(1, rayTracingManager.get());
    VKFramePrepareData framePrepareData;
    framePrepareData.AppendVKCommandBuffer(static_cast<VKCommandBuffer*>(&cmd));

    VkFenceCreateInfo fenceCreateInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, 0};
    VkFence fence;
    vkCreateFence(device.handle, &fenceCreateInfo, nullptr, &fence);

    VkCommandBufferAllocateInfo cmdAllocateInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cmdAllocateInfo.commandPool = mainCmdPool;
    cmdAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocateInfo.commandBufferCount = 1;
    VKFrameContext fakeInflightCmd;
    VkCommandBuffer vkcmd;
    vkAllocateCommandBuffers(device.handle, &cmdAllocateInfo, &vkcmd);
    fakeInflightCmd.cmd = vkcmd;
    VKDebugUtils::SetDebugName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)vkcmd, "VKDriver");

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(vkcmd, &beginInfo);

    CmdBufExecutionReport execReport{};
    rg.Execute(framePrepareData, fakeInflightCmd, 0, mainQueue, featureSettings, execReport);
    vkEndCommandBuffer(vkcmd);

    VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &vkcmd;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &transferSignalSemaphore;
    submitInfo.pWaitDstStageMask = &stageMask;

    ENGINE_BEGIN_PROFILE("VKDriver - submit")
    ASSERT(JobSystem::Instance().GetMainThreadID() == std::this_thread::get_id());
    vkQueueSubmit(mainQueue.handle, 1, &submitInfo, fence);

    vkWaitForFences(device.handle, 1, &fence, VK_TRUE, -1);

    vkDestroyFence(device.handle, fence, VK_NULL_HANDLE);
    vkFreeCommandBuffers(device.handle, mainCmdPool, 1, &vkcmd);
}

Gfx::Image* VKDriver::GetImageFromRenderGraph(const Gfx::ImageIdentifier& id)
{
    if (id.GetType() == Gfx::ImageIdentifier::Type::Image)
    {
        return id.GetAsImage();
    }
    else if (id.GetType() == Gfx::ImageIdentifier::Type::ImageView)
    {
        return &id.GetAsImageView()->GetImage();
    }
    else if (id.GetType() == Gfx::ImageIdentifier::Type::Handle)
    {
        return commandBufferProcessor->GetImage(id.GetAsUUID());
    }
    return nullptr;
}

std::unique_ptr<CommandBuffer> VKDriver::CreateCommandBuffer()
{
    return std::unique_ptr<CommandBuffer>(new VKCommandBuffer(commandBufferProcessor.get()));
}

std::unique_ptr<RayTracingContext> VKDriver::CreateRayTracingContext()
{
    return std::unique_ptr<VKRayTracingContext>(new VKRayTracingContext(rayTracingManager.get(), driverMutex));
}

void VKDriver::AppendOnCompleteCallback(const std::function<void()>& callback)
{
    frameContexts[currentInflightIndex].onCompleteCallbacks.push_back(callback);
}

Window* VKDriver::CreateExtraWindow(SDL_Window* window)
{
    auto newWindow = std::make_unique<VKWindow>(window, context->driverConfig.swapchainImageCount);
    Window* tmp = newWindow.get();
    extraWindows.push_back(std::move(newWindow));
    return tmp;
}

void VKDriver::DestroyExtraWindow(Window* window)
{
    auto iter = std::find_if(extraWindows.begin(), extraWindows.end(), [window](auto& w)
                             { return w.get() == window; });
    if (iter != extraWindows.end())
    {
        extraWindows.erase(iter);
    }
}

void VKDriver::CaptureFrameRenderDoc(bool nextFrame)
{
    captureFrame = true;
    if (!captureFrameBegin && !nextFrame)
    {
        BeginFrameCapture();
    }
}

void VKDriver::WaitForCurrentInflightCmd()
{
    vkWaitForFences(device.handle, 1, &frameContexts[currentInflightIndex].cmdFence, true, -1);
    for (auto& f : frameContexts[currentInflightIndex].onCompleteCallbacks)
    {
        f();
    }
    memAllocator->GPUFrameFinished(frameContexts[currentInflightIndex].frameIndex);
    frameContexts[currentInflightIndex].onCompleteCallbacks.clear();
}

void VKDriver::QueryGPUTimestamp(CmdBufExecutionReport& execReport)
{
    // TODO(perf): we shouldn't wait in query (VK_QUERY_RESULT_WAIT_BIT)
    timestamps.resize(execReport.timestampQueryLabels.size());
    auto result = vkGetQueryPoolResults(
        device.handle,
        frameContexts[currentInflightIndex].timestapQueryPool,
        0,
        execReport.timestampQueryLabels.size(),
        timestamps.size() * sizeof(TimestampQuery),
        timestamps.data(),
        sizeof(TimestampQuery),
        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT
    );
    int timestampIdx = 0;
    for (auto& t : timestamps)
    {
        execReport.timestampQueryLabels[timestampIdx].timestamp =
            t.timestamp * static_cast<uint64_t>(
                              gpu.physicalDeviceProperties.limits.timestampPeriod
                          ); // I am not sure if this cast is safe, I assume all timestampPeriod is integer even tho the
                             // type is a float
        timestampIdx += 1;
    }

    if (execReport.timestampQueryLabels.size() >= 2)
    {
        profiler.BeginFrameManual(execReport.timestampQueryLabels.front().timestamp);
        for (auto& label : execReport.timestampQueryLabels)
        {
            if (label.type == TimestampLabelType::Begin)
            {
                profiler.BeginManual(label.name, label.timestamp);
            }
            else if (label.type == TimestampLabelType::End)
            {
                profiler.EndManual(label.timestamp);
            }
        }
        profiler.EndFrameManual(execReport.timestampQueryLabels.back().timestamp);
    }
}

void VKDriver::SetGPUProfilerEnabled(bool enabled)
{
    featureSettings.enableGPUProfiling = enabled && gpuFeatures.timestampPeriod;
}

void VKDriver::SetWin32WindowInteropTexture(const void* sharedHandle, int2 size)
{
#if WIN32
    needPresent = false;
    swapchain.AsWin32WindowInteropTexture(sharedHandle, size);
#endif
}

void VKDriver::UnsetWin32WindowInteropTexture(int2 size)
{
#if WIN32
    needPresent = true;
    swapchain.CreateOrOverrideSwapChain(surface, context->driverConfig.swapchainImageCount, size.x, size.y);
#endif
}

void VKDriver::BeginFrameCapture()
{
#if __WIN32__
    if (captureFrame && IsRenderDocInitialized())
    {
        if (renderDocAPI->IsTargetControlConnected())
            renderDocAPI->ShowReplayUI();
        else
            renderDocAPI->LaunchReplayUI(1, NULL);

        renderDocAPI->StartFrameCapture(
            RENDERDOC_DEVICEPOINTER_FROM_VKINSTANCE(instance.handle),
            sdlInfo->wmInfo.info.win.window
        );
        captureFrameBegin = true;
    }
#endif
}
} // namespace Gfx
