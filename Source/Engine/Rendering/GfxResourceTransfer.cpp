#include "GfxResourceTransfer.hpp"
#include "GfxDriver/GfxDriver.hpp"

namespace Engine::Internal
{
UniPtr<GfxResourceTransfer> GfxResourceTransfer::instance = nullptr;

GfxResourceTransfer::GfxResourceTransfer()
{
    Gfx::Buffer::CreateInfo createInfo{.usages = Gfx::BufferUsage::Transfer_Src,
                                       .size = 1024 * 1024 * 30,
                                       .visibleInCPU = true,
                                       .debugName = "resource transfer staging buffer"};

    stagingBuffer = GetGfxDriver()->CreateBuffer(createInfo);
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
    pendingBuffers.clear();

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
    pendingImages.clear();
}
} // namespace Engine::Internal
