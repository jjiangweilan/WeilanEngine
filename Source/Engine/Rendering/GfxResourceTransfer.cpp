#include "GfxResourceTransfer.hpp"
#include "GfxDriver/CommandQueue.hpp"
#include "GfxDriver/GfxDriver.hpp"

namespace Engine::Internal
{
UniPtr<GfxResourceTransfer> GfxResourceTransfer::instance = nullptr;

GfxResourceTransfer::GfxResourceTransfer()
{
    Gfx::Buffer::CreateInfo createInfo{.usages = Gfx::BufferUsage::Transfer_Src,
                                       .size = stagingBufferSize,
                                       .visibleInCPU = true,
                                       .debugName = "resource transfer staging buffer"};

    stagingBuffer = GetGfxDriver()->CreateBuffer(createInfo);

    Gfx::CommandPool::CreateInfo cmdPoolCreateInfo;
    cmdPoolCreateInfo.queueFamilyIndex = GetGfxDriver()->GetQueue(QueueType::Main)->GetFamilyIndex();
    cmdPool = GetGfxDriver()->CreateCommandPool(cmdPoolCreateInfo);
    cmdBuf = std::move(cmdPool->AllocateCommandBuffers(Gfx::CommandBufferType::Primary, 1)[0]);
    fence = GetGfxDriver()->CreateFence({false});
}

void GfxResourceTransfer::QueueTransferCommands(RefPtr<Gfx::CommandBuffer> cmdBuf)
{
    for (auto& [buffer, requests] : pendingBuffers)
    {
        bufferCopyRegions.clear();

        for (auto& request : requests)
        {
            bufferCopyRegions.push_back({request.srcOffset, request.userRequest.bufOffset, request.userRequest.size});
        }

        cmdBuf->CopyBuffer(stagingBuffer, buffer, bufferCopyRegions);
    }

    for (auto& [image, requests] : pendingImages)
    {
        bufferImageCopyRegions.clear();

        for (auto& request : requests)
        {
            float scale = std::pow(2, request.userRequest.subresourceLayers.mipLevel);
            Gfx::BufferImageCopyRegion region;
            region.offset = {0, 0, 0};
            uint32_t width = std::max(1.0f, std::floor(image->GetDescription().width / scale));
            uint32_t height = std::max(1.0f, std::floor(image->GetDescription().height / scale));
            region.extend = {width, height, 1};
            region.layers = request.userRequest.subresourceLayers;
            region.srcOffset = request.srcOffset;
            bufferImageCopyRegions.push_back(region);
        }

        Gfx::GPUBarrier barrier;
        barrier.srcStageMask = Gfx::PipelineStage::All_Commands;
        barrier.dstStageMask = Gfx::PipelineStage::All_Commands;
        barrier.dstAccessMask = Gfx::AccessMask::Memory_Read | Gfx::AccessMask::Memory_Write;
        barrier.srcAccessMask = Gfx::AccessMask::Memory_Read | Gfx::AccessMask::Memory_Write;
        barrier.image = image;
        barrier.imageInfo.dstQueueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED;
        barrier.imageInfo.srcQueueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED;
        barrier.imageInfo.oldLayout = Gfx::ImageLayout::Undefined;
        barrier.imageInfo.newLayout = Gfx::ImageLayout::Transfer_Dst;
        barrier.imageInfo.subresourceRange.baseMipLevel = 0;
        barrier.imageInfo.subresourceRange.levelCount = image->GetDescription().mipLevels;
        barrier.imageInfo.subresourceRange.baseArrayLayer = 0;
        barrier.imageInfo.subresourceRange.layerCount = image->GetDescription().isCubemap ? 6 : 1;

        cmdBuf->Barrier(&barrier, 1);
        cmdBuf->CopyBufferToImage(stagingBuffer, image, bufferImageCopyRegions);

        barrier.srcStageMask = Gfx::PipelineStage::All_Commands;
        barrier.dstStageMask = Gfx::PipelineStage::All_Commands;
        barrier.dstAccessMask = Gfx::AccessMask::Memory_Read | Gfx::AccessMask::Memory_Write;
        barrier.srcAccessMask = Gfx::AccessMask::Memory_Read | Gfx::AccessMask::Memory_Write;
        barrier.image = image;
        barrier.imageInfo.dstQueueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED;
        barrier.imageInfo.srcQueueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED;
        barrier.imageInfo.oldLayout = Gfx::ImageLayout::Transfer_Dst;
        barrier.imageInfo.newLayout = Gfx::ImageLayout::Shader_Read_Only;
        barrier.imageInfo.subresourceRange.baseMipLevel = 0;
        barrier.imageInfo.subresourceRange.levelCount = image->GetDescription().mipLevels;
        barrier.imageInfo.subresourceRange.baseArrayLayer = 0;
        barrier.imageInfo.subresourceRange.layerCount = image->GetDescription().isCubemap ? 6 : 1;

        cmdBuf->Barrier(&barrier, 1);
    }

    pendingBuffers.clear();
    pendingImages.clear();
    stagingBufferOffset = 0;
}

void GfxResourceTransfer::ImmediateSubmit()
{
    cmdPool->ResetCommandPool();
    cmdBuf->Begin();
    QueueTransferCommands(cmdBuf);
    cmdBuf->End();
    RefPtr<Gfx::CommandBuffer> cmdBufs[1] = {cmdBuf};
    GetGfxDriver()->QueueSubmit(GetGfxDriver()->GetQueue(QueueType::Main), cmdBufs, {}, {}, {}, fence);
    GetGfxDriver()->WaitForFence({fence}, true, -1); // TODO: improvement needed
    fence->Reset();
}

void GfxResourceTransfer::Transfer(RefPtr<Gfx::Buffer> buffer, const BufferTransferRequest& request)
{
    assert(request.size < stagingBufferSize);
    if (stagingBufferOffset + request.size > stagingBufferSize)
    {
        ImmediateSubmit();
    }
    memcpy((char*)stagingBuffer->GetCPUVisibleAddress() + stagingBufferOffset, request.data, request.size);
    pendingBuffers[buffer.Get()].push_back({buffer, stagingBufferOffset, request});
    stagingBufferOffset += request.size;
}

void GfxResourceTransfer::Transfer(RefPtr<Gfx::Image> image, const ImageTransferRequest& request)
{
    assert(request.size < stagingBufferSize);

    /* pad the offset because ...
    The Vulkan spec states: If dstImage does not have either a depth/stencil or a multi-planar format, then for
    each element of pRegions, bufferOffset must be a multiple of the format's texel block size
    (https://vulkan.lunarg.com/doc/view/1.3.231.1/windows/1.3-extensions/vkspec.html#VUID-vkCmdCopyBufferToImage-bufferOffset-01558)
    */
    if (stagingBufferOffset != 0)
    {
        int byteSize = Gfx::Utils::MapImageFormatToByteSize(image->GetDescription().format);
        stagingBufferOffset += byteSize - stagingBufferOffset % byteSize;
    }

    if (stagingBufferOffset + request.size > stagingBufferSize)
    {
        ImmediateSubmit();
    }
    memcpy((char*)stagingBuffer->GetCPUVisibleAddress() + stagingBufferOffset, request.data, request.size);
    if (!request.keepData)
        delete (unsigned char*)request.data;
    pendingImages[image.Get()].push_back({image, stagingBufferOffset, request});
    stagingBufferOffset += request.size;
}
} // namespace Engine::Internal
