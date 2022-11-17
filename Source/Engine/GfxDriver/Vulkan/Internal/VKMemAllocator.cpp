#include "VKMemAllocator.hpp"
#include "../VKImage.hpp"
#include "../VKBuffer.hpp"
#include <spdlog/spdlog.h>
#include <cassert>
#define VK_CHECK(x) \
    auto rlt_VK_CHECK = x; \
    assert(rlt_VK_CHECK == VK_SUCCESS);

namespace Engine::Gfx
{
    VKMemAllocator::VKMemAllocator(VkInstance instance, RefPtr<VKDevice> device, VkPhysicalDevice physicalDevice, uint32_t transferQueueIndex) :
        device(device),
        queueFamilyIndex(transferQueueIndex)
    {
        // vma
        VmaAllocatorCreateInfo vmaAllocatorCreateInfo{};
        vmaAllocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_0;
        vmaAllocatorCreateInfo.physicalDevice = physicalDevice;
        vmaAllocatorCreateInfo.device = device->GetHandle();
        vmaAllocatorCreateInfo.instance = instance;

        VK_CHECK(vmaCreateAllocator(&vmaAllocatorCreateInfo, &allocator_vma));
    }


    VKMemAllocator::~VKMemAllocator()
    {
        // release pending staging buffers in commands
        for(auto& f : pendingCommands)
        {
            f(VK_NULL_HANDLE);
        }

        DestroyPendingResources();

        vmaDestroyAllocator(allocator_vma);
    }

    void VKMemAllocator::CreateBuffer(VkBufferCreateInfo& bufferCreateInfo, VmaAllocationCreateInfo& allocationCreateInfo, VkBuffer& buffer, VmaAllocation& allocation, VmaAllocationInfo* allocationInfo)
    {
        VK_CHECK(vmaCreateBuffer(allocator_vma, &bufferCreateInfo, &allocationCreateInfo, &buffer, &allocation, allocationInfo));
    }

    void VKMemAllocator::CreateBuffer(VkBufferCreateInfo& bufferCreateInfo, VkBuffer& buffer, VmaAllocation& allocation, VmaAllocationInfo* allocationInfo)
    {
        VmaAllocationCreateInfo allocationCreateInfo{};
        allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

        VK_CHECK(vmaCreateBuffer(allocator_vma, &bufferCreateInfo, &allocationCreateInfo, &buffer, &allocation, allocationInfo));
    }

    void VKMemAllocator::DestroyBuffer(VkBuffer buffer, VmaAllocation allocation)
    {
        pendingBuffersToDelete.emplace_back(buffer, allocation);
    }

    void VKMemAllocator::DestoryImage(VkImage image, VmaAllocation allocation)
    {
        pendingImagesToDelete.emplace_back(image, allocation);
    }

    void VKMemAllocator::CreateImage(VkImageCreateInfo& imageCreateInfo, VkImage& image, VmaAllocation& allocation, VmaAllocationInfo* allocationInfo)
    {
        VmaAllocationCreateInfo allocationCreateInfo{};
        allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

        VK_CHECK(vmaCreateImage(allocator_vma, &imageCreateInfo, &allocationCreateInfo, &image, &allocation, allocationInfo));
    }

    void VKMemAllocator::UploadImage(RefPtr<VKImage> image, uint32_t imageSize, void* data)
    {
        // create stage buffer
        VmaAllocation allocation;
        VmaAllocationInfo allocationInfo;
        VkBuffer stageBuffer = GetStageBuffer(imageSize, allocation, allocationInfo);
        // copy data to stage buffer

        void* mappedData = nullptr;
        vmaMapMemory(allocator_vma, allocation, &mappedData);
        memcpy((char*)mappedData, data, imageSize);
 
        vmaUnmapMemory(allocator_vma, allocation);

        std::function<void(VkCommandBuffer)> pendingCommand = [=](VkCommandBuffer cmd)
        {
            if (cmd != VK_NULL_HANDLE)
            {
                VkBufferImageCopy region;
                region.bufferOffset = 0;
                region.bufferRowLength = 0;
                region.bufferImageHeight = 0;
                region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                region.imageSubresource.mipLevel = 0;
                region.imageSubresource.baseArrayLayer = 0;
                region.imageSubresource.layerCount = 1;
                region.imageOffset = VkOffset3D{0, 0, 0};
                region.imageExtent = VkExtent3D{
                    image->GetDescription().width,
                    image->GetDescription().height,
                    1
                };

                image->TransformLayoutIfNeeded(
                    cmd, 
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_ACCESS_TRANSFER_WRITE_BIT);

                vkCmdCopyBufferToImage(
                    cmd,
                    stageBuffer,
                    image->GetImage(),
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1,
                    &region);
            }

            this->pendingBuffersToDelete.emplace_back(stageBuffer, allocation);
        };

        pendingCommands.push_back(std::move(pendingCommand));
    }

    VkBuffer VKMemAllocator::GetStageBuffer(uint32_t size,
        VmaAllocation& allocation,
        VmaAllocationInfo& allocationInfo)
    {
        VkBufferCreateInfo bufferCreateInfo{};
        bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCreateInfo.pNext = VK_NULL_HANDLE;
        bufferCreateInfo.flags = 0;
        bufferCreateInfo.size = size;
        bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        bufferCreateInfo.queueFamilyIndexCount = 1;
        bufferCreateInfo.pQueueFamilyIndices = &queueFamilyIndex;

        VmaAllocationCreateInfo allocationCreateInfo{};
        allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

        VkBuffer srcBuf = VK_NULL_HANDLE;
        VK_CHECK(vmaCreateBuffer(allocator_vma, &bufferCreateInfo, &allocationCreateInfo, &srcBuf, &allocation, &allocationInfo));
        return srcBuf;
    }

    void VKMemAllocator::UploadBuffer(RefPtr<VKBuffer> buffer, uint32_t dstOffset, size_t dstSize, DataRange dataRange[], uint32_t rangeCount)
    {
        // create stage buffer
        VmaAllocation allocation;
        VmaAllocationInfo allocationInfo;

        VkBuffer stageBuffer = GetStageBuffer(dstSize, allocation, allocationInfo); // TODO: we can use a persistent staging buffer, and may be use 16 bit alignment boundary?

        // copy data to stage buffer
        void* mappedData;
        vmaMapMemory(allocator_vma, allocation, &mappedData);
        for(uint32_t i = 0; i < rangeCount ; ++i)
        {
            DataRange* range = dataRange + i;
            memcpy((char*)mappedData + range->offsetInSrc, range->data, range->size);
        }
        vmaUnmapMemory(allocator_vma, allocation);

        std::function<void(VkCommandBuffer)> pendingCommand = [=](VkCommandBuffer cmd)
        {
            if (cmd != VK_NULL_HANDLE)
            {
                VkBufferCopy region
                {
                    0, dstOffset, dstSize
                };

                buffer->PutMemoryBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);

                vkCmdCopyBuffer(cmd, stageBuffer, buffer->GetVKBuffer(), 1, &region);
            }

            this->pendingBuffersToDelete.emplace_back(stageBuffer, allocation);
        };

        pendingCommands.push_back(std::move(pendingCommand));
    }

    void VKMemAllocator::RecordPendingCommands(VkCommandBuffer cmd)
    {
        for(auto& f : pendingCommands)
        {
            f(cmd);
        }

        pendingCommands.clear();
    }

    void VKMemAllocator::DestroyPendingResources()
    {
        for(auto& p : pendingBuffersToDelete)
        {
            vmaDestroyBuffer(allocator_vma, p.first, p.second);
        }

        for(auto& p : pendingImagesToDelete)
        {
            vmaDestroyImage(allocator_vma, p.first, p.second);
        }

        pendingBuffersToDelete.clear();
        pendingImagesToDelete.clear();
    }

}
