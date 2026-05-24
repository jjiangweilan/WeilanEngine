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
static VkBufferUsageFlags MapTemporaryBufferUsage(TemporaryBufferUsage usage, bool hostVisible)
{
    VkBufferUsageFlags flags = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    switch (usage)
    {
    case TemporaryBufferUsage::Uniform:
        flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        break;
    case TemporaryBufferUsage::Storage:
        flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        break;
    case TemporaryBufferUsage::Index:
        flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        break;
    case TemporaryBufferUsage::Indirect:
        flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
        break;
    case TemporaryBufferUsage::AccelerationStructure:
        flags |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        break;
    case TemporaryBufferUsage::TransferSrc:
        flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        flags &= ~VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        break;
    }

    if (hostVisible)
    {
        flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    }

    return flags;
}
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

void VKMemAllocator::DestroyImage(VkImage image, VmaAllocation allocation)
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
    uint64_t size, uint64_t alignment, TemporaryBufferUsage usage, bool hostVisible
)
{
    ASSERT(size > 0);
    ASSERT(alignment > 0);

    // Try to allocate from the current block first
    Block* selectedBlock = nullptr;
    AllocationHandle rtn{};
    bool isSameFrame = currentBlock && currentBlock->frameIndex == currentFrameIndex;
    bool blockReady = currentBlock &&
                      (currentBlock->usage == usage) &&
                      (currentBlock->hostVisible == hostVisible) &&
                      (!currentBlock->inflight || isSameFrame);
    if (blockReady &&
        Math::AlignMemory(currentBlock->offset, alignment) + size <= currentBlock->size)
    {
        selectedBlock = currentBlock;
    }

    // Current block is full or usage mismatch, search for another block with compatible usage
    if (selectedBlock == nullptr)
    {
        for (auto& block : blocks)
        {
            bool sameFrame = block.frameIndex == currentFrameIndex;
            bool compatible = (block.usage == usage) && (block.hostVisible == hostVisible) &&
                              (!block.inflight || sameFrame);
            if (compatible)
            {
                uint64_t alignedOffset = Math::AlignMemory(block.offset, alignment);
                if (alignedOffset + size > block.size)
                    continue;

                selectedBlock = &block;
                break;
            }
        }
    }

    // No suitable block found, create a new one
    if (selectedBlock == nullptr)
    {
        uint64_t blockSize = std::max(size, static_cast<uint64_t>(16 * 1024 * 1024));
        Block& newBlock = CreateBlock(blockSize, usage, hostVisible);
        selectedBlock = &newBlock;
    }

    // this should be always true
    if (selectedBlock != nullptr)
    {
        uint64_t alignedOffset = Math::AlignMemory(selectedBlock->offset, alignment);
        ASSERT(alignedOffset + size <= selectedBlock->size);

        rtn.buffer = selectedBlock->buffer;
        rtn.deviceAddress = selectedBlock->deviceAddress + alignedOffset;
        rtn.mappedData = selectedBlock->mappedData ? static_cast<uint8_t*>(selectedBlock->mappedData) + alignedOffset : nullptr;
        rtn.offset = alignedOffset;
        rtn.size = size;
        rtn.block = selectedBlock;

        selectedBlock->offset = alignedOffset + size;
        selectedBlock->inflight = true;
        selectedBlock->frameIndex = currentFrameIndex;
        selectedBlock->lifetime = 0;

        // cache the selected block
        currentBlock = selectedBlock;
    }

    return rtn;
}

void VKMemAllocator::ScratchBuffer::FrameFinished(int frameIndex)
{
    for (auto& block : blocks)
    {
        if (block.frameIndex <= frameIndex && block.inflight)
        {
            block.offset = 0;
            block.inflight = false;
            block.lifetime = 0;
        }
    }

    for (auto it = blocks.begin(); it != blocks.end();)
    {
        if (it->frameIndex <= frameIndex && !it->inflight)
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

VKMemAllocator::ScratchBuffer::Block& VKMemAllocator::ScratchBuffer::CreateBlock(uint64_t size, TemporaryBufferUsage usage, bool hostVisible)
{
    Block block;
    block.size = size;
    block.usage = usage;
    block.hostVisible = hostVisible;

    VkBufferUsageFlags blockUsage = MapTemporaryBufferUsage(usage, hostVisible);
    block.bufferUsages = blockUsage;

    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

    if (hostVisible)
    {
        allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }
    else
    {
        allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    }

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
    block.lifetime = 0;

    blocks.push_back(block);
    return blocks.back();
}
} // namespace Gfx
