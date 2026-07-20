#include "VKDataUploader.hpp"
#include "Engine/Core/JobSystem.hpp"
#include "Engine/Core/Profiler/Profiler.hpp"
#include "VKBuffer.hpp"
#include "VKDriver.hpp"
#include <spdlog/spdlog.h>

namespace Gfx
{
namespace
{
struct UploadFinalState
{
    VkPipelineStageFlags2 stages;
    VkAccessFlags2 access;
};

UploadFinalState GetUploadFinalState(VkImageLayout layout)
{
    switch (layout)
    {
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
            return {VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_SHADER_READ_BIT};
        case VK_IMAGE_LAYOUT_GENERAL:
            return {
                VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT
            };
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            return {VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT};
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            return {VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT};
        default:
            return {
                VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT
            };
    }
}

void PutImageBarriers(VkCommandBuffer cmd, std::span<const VkImageMemoryBarrier2> imageBarriers)
{
    if (imageBarriers.empty())
        return;

    VkDependencyInfo dependencyInfo{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dependencyInfo.imageMemoryBarrierCount = static_cast<uint32_t>(imageBarriers.size());
    dependencyInfo.pImageMemoryBarriers = imageBarriers.data();
    vkCmdPipelineBarrier2(cmd, &dependencyInfo);
}
}

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

void VKDataUploader::UploadBuffer(const VKBuffer* dst, uint8_t* data, size_t size, size_t dstOffset)
{
    ASSERT(std::this_thread::get_id() == JobSystem::Instance().GetMainThreadID());
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
    ASSERT(std::this_thread::get_id() == JobSystem::Instance().GetMainThreadID());

    auto vkDst = static_cast<VKImage*>(dst);

    size_t byteSize = MapGfxFormatToBlockByteSize(vkDst->GetDescription().format);
    size_t align = (byteSize - (takingOffCmd.endOffset % byteSize)) % byteSize;

    if (size + align > stagingBufferSize)
    {
        SPDLOG_ERROR("failed to upload buffer: buffer size is larger than 48 MB");
        return;
    }

    if (!EnsureEnoughSizeForUpload(takingOffCmd, size + align))
    {
        // offset may change, recalculate alignment
        align = (byteSize - (takingOffCmd.endOffset % byteSize)) % byteSize;
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
            ObjPtr<VKImage>((Object*)vkDst),
            (uint32_t)(vkDst->GetDescription().width * scale),
            (uint32_t)(vkDst->GetDescription().height * scale),
            (uint32_t)glm::max((vkDst->GetDescription().depth * scale), 1.0f),
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

void VKDataUploader::FlushCachedUpload()
{
    for (auto& cachedImageUpload : cachedImageUploads)
    {
        if (cachedImageUpload.dst != nullptr)
        {
            UploadImage(cachedImageUpload.dst, cachedImageUpload.data.data(), cachedImageUpload.size, cachedImageUpload.mipLevel, cachedImageUpload.arrayLayer, cachedImageUpload.aspect, cachedImageUpload.finalLayout);
        }
    }
    for (auto& cachedBufferUpload : cachedBufferUploads)
    {
        UploadBuffer(cachedBufferUpload.dst, cachedBufferUpload.data.data(), cachedBufferUpload.size, cachedBufferUpload.dstOffset);
    }

    cachedImageUploads.clear();
    cachedBufferUploads.clear();
}

void VKDataUploader::UploadAllPending(
    VkSemaphore signalSemaphore, VkSemaphore waitSemaphore, VkPipelineStageFlags waitStages
)
{
    ENGINE_SCOPED_PROFILE("VKDataUploader::UploadAllPending");

    FlushCachedUpload();
    UploadAllPendingInternal(signalSemaphore, waitSemaphore, waitStages);
}

void VKDataUploader::UploadAllPendingInternal(
    VkSemaphore signalSemaphore, VkSemaphore waitSemaphore, VkPipelineStageFlags waitStages
)
{
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
        VKImage* dst = p.dst;
        if (dst == nullptr)
            continue;

        VkBufferImageCopy region{};
        region.bufferOffset = p.srcOffset;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = p.aspect;
        region.imageSubresource.mipLevel = p.mipLevel;
        region.imageSubresource.baseArrayLayer = p.arrayLayer;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = VkOffset3D{0, 0, 0};
        region.imageExtent = VkExtent3D{p.width, p.height, p.depth};

        VkImageSubresourceRange range{
            .aspectMask = p.aspect,
            .baseMipLevel = p.mipLevel,
            .levelCount = 1,
            .baseArrayLayer = p.arrayLayer,
            .layerCount = 1,
        };

        auto toTransferDst = dst->MakeBarrierIfNeeded(
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            range
        );
        PutImageBarriers(takingOffCmd.cmd, toTransferDst);

        vkCmdCopyBufferToImage(
            takingOffCmd.cmd,
            stagingBuffer.handle,
            dst->GetImage(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &region
        );

        const UploadFinalState finalState = GetUploadFinalState(p.finalLayout);
        VkImageMemoryBarrier2 toFinal{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .dstStageMask = finalState.stages,
            .dstAccessMask = finalState.access,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = p.finalLayout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = dst->GetImage(),
            .subresourceRange = range,
        };
        PutImageBarriers(takingOffCmd.cmd, std::span<const VkImageMemoryBarrier2>(&toFinal, 1));

        dst->SetLayout(
            range,
            p.finalLayout,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            finalState.stages,
            finalState.access
        );
        takingOffCmd.imageRefs.push_back(p.dst);
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
    UploadAllPendingInternal(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

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
