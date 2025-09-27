#include "VKDataUploader.hpp"
#include "Profiler/Profiler.hpp"
#include "VKBuffer.hpp"
#include "VKDriver.hpp"
#include <spdlog/spdlog.h>

namespace Gfx
{
VKDataUploader::VKDataUploader(VKDriver* driver) : driver(driver)
{
    stagingBuffer = driver->Driver_CreateBuffer(
        stagingBufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
    );

    VkCommandBufferAllocateInfo rhiCmdAllocateInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    rhiCmdAllocateInfo.commandPool = driver->mainCmdPool;
    rhiCmdAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    rhiCmdAllocateInfo.commandBufferCount = 1;
    vkAllocateCommandBuffers(driver->device.handle, &rhiCmdAllocateInfo, &takingOffCmd.cmd);
    VKDebugUtils::SetDebugName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)takingOffCmd.cmd, "VKDataUploader");
    takingOffCmd.fence = fencePool.Allocate(driver->device.handle);
}

VKDataUploader::~VKDataUploader()
{
    driver->Driver_DestroyBuffer(stagingBuffer);

    WaitForUploadFinish();

    if (takingOffCmd.fence != VK_NULL_HANDLE)
    {
        vkDestroyFence(driver->device.handle, takingOffCmd.fence, VK_NULL_HANDLE);
    }

    while (!inflightCmds.empty())
    {
        auto& cmd = inflightCmds.front();
        vkDestroyFence(driver->device.handle, cmd.fence, VK_NULL_HANDLE);
        inflightCmds.pop();
    }

    for (auto f : fencePool.fences)
        vkDestroyFence(driver->device.handle, f, VK_NULL_HANDLE);
}

void VKDataUploader::UploadBuffer(VKBuffer* dst, uint8_t* data, size_t size, size_t dstOffset)
{
    if (size > stagingBufferSize)
    {
        SPDLOG_ERROR("failed to upload buffer: buffer size is larger than 48 MB");
        return;
    }

    EnsureEnoughSizeForUpload(takingOffCmd, size);

    memcpy((uint8_t*)stagingBuffer.allocationInfo.pMappedData + takingOffCmd.endOffset, data, size);
    pendingBufferUploads.push_back(PendingBufferUpload{dst->GetHandle(), takingOffCmd.endOffset, dstOffset, size});
    takingOffCmd.endOffset += size;
}

void VKDataUploader::UploadImage(
    VKImage* dst,
    uint8_t* data,
    size_t size,
    uint32_t mipLevel,
    uint32_t arayLayer,
    VkImageAspectFlags aspect,
    VkImageLayout finalLayout
)
{
    auto vkDst = static_cast<VKImage*>(dst);

    size_t byteSize = MapGfxFormatToByteSize(vkDst->GetDescription().format);
    size_t align = byteSize - (takingOffCmd.endOffset % byteSize);

    if (size + align > stagingBufferSize)
    {
        SPDLOG_ERROR("failed to upload buffer: buffer size is larger than 48 MB");
        return;
    }

    if (!EnsureEnoughSizeForUpload(takingOffCmd, size + align))
    {
        // offset may change, recalculate alignment
        align = byteSize - (takingOffCmd.endOffset % byteSize);
        if (size + align > stagingBufferSize)
        {
            SPDLOG_ERROR("failed to upload buffer: buffer size is larger than 48 MB");
            return;
        }
    }

    memcpy((uint8_t*)stagingBuffer.allocationInfo.pMappedData + takingOffCmd.endOffset + align, data, size);
    float scale = glm::pow(0.5, mipLevel);
    pendingImageUploads.push_back(
        PendingImageUpload{
            vkDst->GetImage(),
            (uint32_t)(vkDst->GetDescription().width * scale),
            (uint32_t)(vkDst->GetDescription().height * scale),
            takingOffCmd.endOffset + align,
            size,
            mipLevel,
            arayLayer,
            aspect,
            finalLayout
        }
    );

    takingOffCmd.endOffset += align + size;
}

void VKDataUploader::WaitForUploadFinish()
{
    ENGINE_SCOPED_PROFILE("VKDataUploader::WaitForUploadFinish");
    while (!inflightCmds.empty())
    {
        auto& cmd = inflightCmds.front();
        vkWaitForFences(driver->device.handle, 1, &cmd.fence, true, -1);
        vkFreeCommandBuffers(driver->device.handle, driver->mainCmdPool, 1, &cmd.cmd);
        fencePool.Free(driver->device.handle, cmd.fence);
        inflightCmds.pop();
    }
}

void VKDataUploader::UploadAllPending(
    VkSemaphore signalSemaphore, VkSemaphore waitSemaphore, VkPipelineStageFlags waitStages
)
{
    ENGINE_SCOPED_PROFILE("VKDataUploader::UploadAllPending");

    VkCommandBufferAllocateInfo rhiCmdAllocateInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    rhiCmdAllocateInfo.commandPool = driver->mainCmdPool;
    rhiCmdAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    rhiCmdAllocateInfo.commandBufferCount = 1;
    vkAllocateCommandBuffers(driver->device.handle, &rhiCmdAllocateInfo, &takingOffCmd.cmd);
    VKDebugUtils::SetDebugName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)takingOffCmd.cmd, "VKDataUploader");

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(takingOffCmd.cmd, &beginInfo);

    float4 color = {0.3f, 0.26f, 0.72f, 1.0f};
    VKDebugUtils::CmdBeginLabel(takingOffCmd.cmd, "VKDataUploader", &color[0]);

    for (size_t i = 0; i < pendingBufferUploads.size(); ++i)
    {
        auto& p = pendingBufferUploads[i];

        VkBufferCopy region{
            .srcOffset = p.srcOffset,
            .dstOffset = p.dstOffset,
            .size = p.size,
        };
        copyRegions.push_back(region);

        if (i == pendingBufferUploads.size() - 1 || pendingBufferUploads[i + 1].dst != p.dst)
        {
            vkCmdCopyBuffer(takingOffCmd.cmd, stagingBuffer.handle, p.dst, copyRegions.size(), copyRegions.data());
            copyRegions.clear();
        }
    }

    for (size_t i = 0; i < pendingImageUploads.size(); ++i)
    {
        auto& p = pendingImageUploads[i];

        VkBufferImageCopy region;
        region.bufferOffset = p.srcOffset;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = p.aspect;
        region.imageSubresource.mipLevel = p.mipLevel;
        region.imageSubresource.baseArrayLayer = p.arrayLayer;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = VkOffset3D{0, 0, 0};
        region.imageExtent = VkExtent3D{p.width, p.height, 1};

        bufferImageCopies.push_back(region);

        if (i + 1 == pendingImageUploads.size() || pendingImageUploads[i + 1].dst != p.dst)
        {
            barriers.clear();
            for (auto& bic : bufferImageCopies)
            {
                VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.image = p.dst;

                VkImageSubresourceRange range;
                range.aspectMask = bic.imageSubresource.aspectMask;
                range.baseMipLevel = bic.imageSubresource.mipLevel;
                range.levelCount = 1;
                range.baseArrayLayer = bic.imageSubresource.baseArrayLayer;
                range.layerCount = bic.imageSubresource.layerCount;

                barrier.subresourceRange = range;

                barriers.push_back(barrier);
            }

            vkCmdPipelineBarrier(
                takingOffCmd.cmd,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                0,
                VK_NULL_HANDLE,
                0,
                VK_NULL_HANDLE,
                barriers.size(),
                barriers.data()
            );

            vkCmdCopyBufferToImage(
                takingOffCmd.cmd,
                stagingBuffer.handle,
                p.dst,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                bufferImageCopies.size(),
                bufferImageCopies.data()
            );
            bufferImageCopies.clear();

            for (auto& b : barriers)
            {
                b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                b.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
                b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                b.newLayout = p.finalLayout;
            }
            vkCmdPipelineBarrier(
                takingOffCmd.cmd,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                0,
                0,
                VK_NULL_HANDLE,
                0,
                VK_NULL_HANDLE,
                barriers.size(),
                barriers.data()
            );
        }
    }

    VKDebugUtils::CmdEndLabel(takingOffCmd.cmd);
    vkEndCommandBuffer(takingOffCmd.cmd);

    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.waitSemaphoreCount = waitSemaphore == VK_NULL_HANDLE ? 0 : 1;
    submitInfo.pWaitSemaphores = &waitSemaphore;
    submitInfo.pWaitDstStageMask = &waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &takingOffCmd.cmd;
    submitInfo.signalSemaphoreCount = signalSemaphore == VK_NULL_HANDLE ? 0 : 1;
    submitInfo.pSignalSemaphores = &signalSemaphore;
    vkQueueSubmit(driver->mainQueue.handle, 1, &submitInfo, takingOffCmd.fence);

    pendingBufferUploads.clear();
    pendingImageUploads.clear();
    inflightCmds.push(takingOffCmd);

    while (!inflightCmds.empty())
    {
        auto& cmd = inflightCmds.front();
        auto result = vkGetFenceStatus(driver->device.handle, cmd.fence);
        if (result == VK_SUCCESS)
        {
            vkFreeCommandBuffers(driver->device.handle, driver->mainCmdPool, 1, &cmd.cmd);
            fencePool.Free(driver->device.handle, cmd.fence);
            inflightCmds.pop();
        }
        else
            break;
    }

    takingOffCmd =
        {VK_NULL_HANDLE, fencePool.Allocate(driver->device.handle), takingOffCmd.endOffset, takingOffCmd.endOffset};
}

bool VKDataUploader::EnsureEnoughSizeForUpload(InflightUploadingCmd& cmd, size_t size)
{
    // check the tail
    size_t head = inflightCmds.empty() ? 0 : inflightCmds.front().startOffset;
    size_t freeSize = cmd.endOffset < head ? head - cmd.endOffset : stagingBufferSize - cmd.endOffset;
    bool canContinue = size < freeSize;
    if (canContinue)
        return true;

    // we can't continue because there isn't enought room for next upload
    // first we upload what we have scheduled so far
    UploadAllPending(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

    // check if we have enough room from beginning to head
    if (head < cmd.endOffset && head > size)
    {
        // upload all pending takes care of creating a new takingOffCmd but it lacks information of if there is enough
        // space for next upload so we need to override it
        takingOffCmd.endOffset = 0;
        takingOffCmd.startOffset = 0;
    }
    else
    {
        // there is no empty space anywhere, wait for all uploads
        // to finish and reset the command buffer
        // Note: a better approach is to wait front inflightCmds until there is enough space for next upload. No need to
        // wait all inflightCmds
        WaitForUploadFinish();

        fencePool.Free(driver->device.handle, takingOffCmd.fence);
        takingOffCmd = {VK_NULL_HANDLE, fencePool.Allocate(driver->device.handle), 0, 0};
    }

    return false;
}
} // namespace Gfx
