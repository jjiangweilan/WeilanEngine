#include "VKDataUploader.hpp"
#include "../VKBuffer.hpp"
#include "../VKDriver.hpp"
#include "GfxDriver/Vulkan/Internal/VKEnumMapper.hpp"
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
    vkAllocateCommandBuffers(driver->device.handle, &rhiCmdAllocateInfo, &cmd);

    VkFenceCreateInfo fenceCreateInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // pipeline waits for the cmd to be finished before it
    vkCreateFence(driver->device.handle, &fenceCreateInfo, VK_NULL_HANDLE, &fence);
}

VKDataUploader::~VKDataUploader()
{
    driver->Driver_DestroyBuffer(stagingBuffer);

    vkDestroyFence(driver->device.handle, fence, VK_NULL_HANDLE);
}

void VKDataUploader::UploadBuffer(VKBuffer* dst, uint8_t* data, size_t size, size_t dstOffset)
{
    if (size > stagingBufferSize)
    {
        SPDLOG_ERROR("failed to upload buffer: buffer size is larger than 48 MB");
        return;
    }

    if (offset + size > stagingBufferSize)
    {
        UploadAllPending(VK_NULL_HANDLE);
        vkWaitForFences(driver->device.handle, 1, &fence, true, -1);
    }

    memcpy((uint8_t*)stagingBuffer.allocationInfo.pMappedData + offset, data, size);
    pendingBufferUploads.push_back(PendingBufferUpload{dst->GetHandle(), offset, dstOffset, size});
    offset += size;
}

void VKDataUploader::UploadImage(
    VKImage* dst, uint8_t* data, size_t size, uint32_t mipLevel, uint32_t arayLayer, VkImageAspectFlags aspect
)
{
    auto vkDst = static_cast<VKImage*>(dst);

    size_t byteSize = MapImageFormatToByteSize(vkDst->GetDescription().format);
    size_t align = byteSize - (offset % byteSize);

    if (size + align > stagingBufferSize)
    {
        SPDLOG_ERROR("failed to upload buffer: buffer size is larger than 48 MB");
        return;
    }

    if (offset + size + align > stagingBufferSize)
    {
        UploadAllPending(VK_NULL_HANDLE);
        vkWaitForFences(driver->device.handle, 1, &fence, true, -1);
    }

    memcpy((uint8_t*)stagingBuffer.allocationInfo.pMappedData + offset + align, data, size);
    pendingImageUploads.push_back(PendingImageUpload{
        vkDst->GetImage(),
        vkDst->GetDescription().width,
        vkDst->GetDescription().height,
        offset + align,
        size,
        mipLevel,
        arayLayer,
        aspect});

    offset += align + size;
}

void VKDataUploader::UploadAllPending(VkSemaphore signalSemaphore)
{
    vkWaitForFences(driver->device.handle, 1, &fence, true, -1);
    vkResetFences(driver->device.handle, 1, &fence);
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

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
            vkCmdCopyBuffer(cmd, stagingBuffer.handle, p.dst, copyRegions.size(), copyRegions.data());
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

        if (i + 1 == pendingImageUploads.size() - 1 || pendingImageUploads[i + 1].dst != p.dst)
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
                cmd,
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
                cmd,
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
                b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }
            vkCmdPipelineBarrier(
                cmd,
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

    vkEndCommandBuffer(cmd);
    pendingBufferUploads.clear();
    pendingImageUploads.clear();
    offset = 0;

    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.waitSemaphoreCount = 0;
    submitInfo.pWaitSemaphores = VK_NULL_HANDLE;
    submitInfo.pWaitDstStageMask = VK_NULL_HANDLE;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    submitInfo.signalSemaphoreCount = signalSemaphore == VK_NULL_HANDLE ? 0 : 1;
    submitInfo.pSignalSemaphores = &signalSemaphore;
    vkQueueSubmit(driver->mainQueue.handle, 1, &submitInfo, fence);
}
} // namespace Gfx
