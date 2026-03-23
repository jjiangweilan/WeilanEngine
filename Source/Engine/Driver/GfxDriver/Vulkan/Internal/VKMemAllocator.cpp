#include "VKMemAllocator.hpp"
#include "../VKBuffer.hpp"
#include "../VKCommon.hpp"
#include "../VKContext.hpp"
#include "../VKImage.hpp"
#include "Engine/Library/Assert.hpp"
#include <spdlog/spdlog.h>
#define VK_CHECK(x)        \
    auto rlt_VK_CHECK = x; \
    ASSERT(rlt_VK_CHECK == VK_SUCCESS);

namespace Gfx
{
VKMemAllocator::VKMemAllocator(
    VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice, uint32_t transferQueueIndex
)
    : device(device), queueFamilyIndex(transferQueueIndex), pendingBuffers(), pendingImages()
{
    // vma
    VmaVulkanFunctions vulkanFunctions = {};
    vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
    vulkanFunctions.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
    vulkanFunctions.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
    vulkanFunctions.vkAllocateMemory = vkAllocateMemory;
    vulkanFunctions.vkFreeMemory = vkFreeMemory;
    vulkanFunctions.vkMapMemory = vkMapMemory;
    vulkanFunctions.vkUnmapMemory = vkUnmapMemory;
    vulkanFunctions.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
    vulkanFunctions.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
    vulkanFunctions.vkBindBufferMemory = vkBindBufferMemory;
    vulkanFunctions.vkBindImageMemory = vkBindImageMemory;
    vulkanFunctions.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
    vulkanFunctions.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
    vulkanFunctions.vkCreateBuffer = vkCreateBuffer;
    vulkanFunctions.vkDestroyBuffer = vkDestroyBuffer;
    vulkanFunctions.vkCreateImage = vkCreateImage;
    vulkanFunctions.vkDestroyImage = vkDestroyImage;
    vulkanFunctions.vkCmdCopyBuffer = vkCmdCopyBuffer;
    vulkanFunctions.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2;
    vulkanFunctions.vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2;
    vulkanFunctions.vkBindBufferMemory2KHR = vkBindBufferMemory2;
    vulkanFunctions.vkBindImageMemory2KHR = vkBindImageMemory2;
    vulkanFunctions.vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2;

    VmaAllocatorCreateInfo vmaAllocatorCreateInfo{};
    vmaAllocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    vmaAllocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaAllocatorCreateInfo.physicalDevice = physicalDevice;
    vmaAllocatorCreateInfo.device = device;
    vmaAllocatorCreateInfo.instance = instance;
    vmaAllocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;

    VK_CHECK(vmaCreateAllocator(&vmaAllocatorCreateInfo, &allocator_vma));

    scratchBuffer.Init(allocator_vma);
}

VKMemAllocator::~VKMemAllocator()
{
    scratchBuffer.Destroy();
    DestroyPendingResources(true);
    vmaDestroyAllocator(allocator_vma);
}

void VKMemAllocator::CreateBuffer(
    VkBufferCreateInfo& bufferCreateInfo,
    VmaAllocationCreateInfo& allocationCreateInfo,
    VkBuffer& buffer,
    VmaAllocation& allocation,
    VmaAllocationInfo* allocationInfo
)
{
    VK_CHECK(
        vmaCreateBuffer(allocator_vma, &bufferCreateInfo, &allocationCreateInfo, &buffer, &allocation, allocationInfo)
    );
}

void VKMemAllocator::CreateBuffer(
    VkBufferCreateInfo& bufferCreateInfo, VkBuffer& buffer, VmaAllocation& allocation, VmaAllocationInfo* allocationInfo
)
{
    VmaAllocationCreateInfo allocationCreateInfo{};
    allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

    VK_CHECK(
        vmaCreateBuffer(allocator_vma, &bufferCreateInfo, &allocationCreateInfo, &buffer, &allocation, allocationInfo)
    );
}

void VKMemAllocator::CreateImage(
    VkImageCreateInfo& imageCreateInfo, VkImage& image, VmaAllocation& allocation, VmaAllocationInfo* allocationInfo
)
{
    VmaAllocationCreateInfo allocationCreateInfo{};
    allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

    VK_CHECK(vmaCreateImage(allocator_vma, &imageCreateInfo, &allocationCreateInfo, &image, &allocation, allocationInfo));
}

VkBuffer VKMemAllocator::GetStageBuffer(uint32_t size, VmaAllocation& allocation, VmaAllocationInfo& allocationInfo)
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
    VK_CHECK(
        vmaCreateBuffer(allocator_vma, &bufferCreateInfo, &allocationCreateInfo, &srcBuf, &allocation, &allocationInfo)
    );
    return srcBuf;
}

void VKMemAllocator::DestroyBuffer(VkBuffer buffer, VmaAllocation allocation)
{
    pendingBuffers.push_back({buffer, allocation, -1});
}

void VKMemAllocator::DestoryImage(VkImage image, VmaAllocation allocation)
{
    pendingImages.push_back({image, allocation, -1});
}

template <class T, class F>
void VKMemAllocator::DestroyPendingResourcesOfType(std::list<Info>& resources, F f, bool destroyAll)
{
    for (auto curr = resources.begin(); curr != resources.end();)
    {
        if (curr->frameCount++ > 5 || destroyAll)
        {
            f(allocator_vma, (T)curr->ptr, curr->allocation);
            auto tmp = curr;
            curr++;
            resources.erase(tmp);
        }
        else
            curr++;
    }
}

void VKMemAllocator::DestroyPendingResources(bool destroyAll)
{
    DestroyPendingResourcesOfType<VkBuffer>(pendingBuffers, vmaDestroyBuffer, destroyAll);
    DestroyPendingResourcesOfType<VkImage>(pendingImages, vmaDestroyImage, destroyAll);
}

void VKMemAllocator::ScratchBuffer::Init(VmaAllocator allocator)
{
    allocator_vma = allocator;
}

void VKMemAllocator::ScratchBuffer::Destroy()
{
    for (auto& block : blocks)
    {
        vmaDestroyBuffer(allocator_vma, block.buffer, block.allocation);
    }
    blocks.clear();
}

VKMemAllocator::ScratchBuffer::AllocationHandle VKMemAllocator::ScratchBuffer::Allocate(
    uint32_t size, uint32_t alignment, ScratchBufferUsage usage
)
{
    // Try to allocate from the current block first
    Block* selectedBlock = nullptr;
    AllocationHandle rtn{};
    if (currentBlock && (currentBlock->usage == usage) &&
        Math::AlignMemory(currentBlock->offset, alignment) + size <= currentBlock->size)
    {
        selectedBlock = currentBlock;
    }

    // Current block is full or usage mismatch, search for another non-inflight block with compatible usage
    if (selectedBlock == nullptr)
    {
        for (auto& block : blocks)
        {
            if (block.usage == usage && !block.inflight)
            {
                uint32_t offset = Math::AlignMemory(block.offset, alignment);
                if (offset + size > block.size)
                    continue;

                selectedBlock = &block;
                break;
            }
        }
    }

    // No suitable block found, create a new one
    if (selectedBlock == nullptr)
    {
        uint32_t blockSize = std::max(size, 16u * 1024 * 1024);
        Block& newBlock = CreateBlock(blockSize, usage);
        selectedBlock = &newBlock;
    }

    // this should be always true
    if (selectedBlock != nullptr)
    {
        uint32_t offset = Math::AlignMemory(selectedBlock->offset, alignment);
        ASSERT(offset + size <= selectedBlock->size);

        rtn.buffer = selectedBlock->buffer;
        rtn.deviceAddress = selectedBlock->deviceAddress + offset;
        rtn.mappedData = selectedBlock->mappedData ? (uint8_t*)selectedBlock->mappedData + offset : nullptr;
        rtn.block = selectedBlock;

        selectedBlock->offset = offset + size;
        selectedBlock->frameIndex = currentFrameIndex;

        // cache the selected block
        currentBlock = selectedBlock;
    }

    return rtn;
}

void VKMemAllocator::ScratchBuffer::FrameFinished(int frameIndex)
{
    for (auto& block : blocks)
    {
        if (block.frameIndex <= frameIndex)
        {
            block.offset = 0;
        }
    }

    for (auto it = blocks.begin(); it != blocks.end();)
    {
        if (it->frameIndex <= frameIndex)
        {
            if (++it->lifetime > 5)
            {
                if (currentBlock == &(*it))
                    currentBlock = nullptr;
                vmaDestroyBuffer(allocator_vma, it->buffer, it->allocation);
                it = blocks.erase(it);
                continue;
            }
        }
        it++;
    }
}

VKMemAllocator::ScratchBuffer::Block& VKMemAllocator::ScratchBuffer::CreateBlock(uint32_t size, ScratchBufferUsage usage)
{
    Block block;
    block.size = size;

    VkBufferUsageFlags blockUsage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

    if (usage == ScratchBufferUsage::GPUScratchBuffer)
    {
        blockUsage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    }
    else if (usage == ScratchBufferUsage::HostVisibleScatchBuffer)
    {
        blockUsage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
        allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }
    block.bufferUsages = blockUsage;

    VkBufferCreateInfo bufferCreateInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferCreateInfo.size = size;
    bufferCreateInfo.usage = blockUsage;

    VK_CHECK(vmaCreateBuffer(allocator_vma, &bufferCreateInfo, &allocCreateInfo, &block.buffer, &block.allocation, nullptr));

    if (allocCreateInfo.flags & VMA_ALLOCATION_CREATE_MAPPED_BIT)
    {
        VmaAllocationInfo allocInfo;
        vmaGetAllocationInfo(allocator_vma, block.allocation, &allocInfo);
        block.mappedData = allocInfo.pMappedData;
    }

    auto device = VKContext::Instance()->device;
    VkBufferDeviceAddressInfo addressInfo = {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
    addressInfo.buffer = block.buffer;
    block.deviceAddress = vkGetBufferDeviceAddress(device, &addressInfo);

    blocks.push_back(block);
    return blocks.back();
}
} // namespace Gfx
