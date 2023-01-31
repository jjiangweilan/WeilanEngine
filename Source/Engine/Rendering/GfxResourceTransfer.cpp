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
    cmdBuf = std::move(cmdPool->AllocateCommandBuffers(CommandBufferType::Primary, 1)[0]);
    fence = GetGfxDriver()->CreateFence({false});
}

void GfxResourceTransfer::QueueTransferCommands(RefPtr<CommandBuffer> cmdBuf)
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
            BufferImageCopyRegion region;
            region.offset = {0, 0, 0};
            region.extend = {image->GetDescription().width, image->GetDescription().height, 1};
            region.range = request.userRequest.subresourceRange;
            region.srcOffset = request.srcOffset;
            bufferImageCopyRegions.push_back(region);
        }

        GPUBarrier barrier;
        barrier.srcStageMask = Gfx::PipelineStage::All_Commands;
        barrier.dstStageMask = Gfx::PipelineStage::All_Commands;
        barrier.dstAccessMask = Gfx::AccessMask::Memory_Read | Gfx::AccessMask::Memory_Write;
        barrier.srcAccessMask = Gfx::AccessMask::Memory_Read | Gfx::AccessMask::Memory_Write;
        barrier.image = image;
        barrier.imageInfo.dstQueueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED;
        barrier.imageInfo.srcQueueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED;
        barrier.imageInfo.oldLayout = Gfx::ImageLayout::Undefined;
        barrier.imageInfo.newLayout = Gfx::ImageLayout::Transfer_Dst;

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
    RefPtr<CommandBuffer> cmdBufs[1] = {cmdBuf};
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
    if (stagingBufferOffset + request.size > stagingBufferSize)
    {
        ImmediateSubmit();
    }
    memcpy((char*)stagingBuffer->GetCPUVisibleAddress() + stagingBufferOffset, request.data, request.size);
    if (!request.keepData)
        delete request.data;
    pendingImages[image.Get()].push_back({image, stagingBufferOffset, request});
    stagingBufferOffset += request.size;
}
} // namespace Engine::Internal
