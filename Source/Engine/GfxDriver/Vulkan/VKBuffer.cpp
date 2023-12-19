#include "VKBuffer.hpp"
#include "GfxDriver/Vulkan/Internal/VKDevice.hpp"
#include "GfxDriver/Vulkan/Internal/VKMemAllocator.hpp"
#include "GfxDriver/Vulkan/VKContext.hpp"
#include "VKDebugUtils.hpp"
// reference: https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/usage_patterns.html
namespace Gfx
{
VkBufferUsageFlags MapBufferUsage(BufferUsageFlags usageIn)
{
    VkBufferUsageFlags usage = 0;
    if (HasFlag(usageIn, BufferUsage::Transfer_Dst))
        usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (HasFlag(usageIn, BufferUsage::Transfer_Src))
        usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (HasFlag(usageIn, BufferUsage::Uniform_Texel))
        usage |= VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
    if (HasFlag(usageIn, BufferUsage::Storage_Texel))
        usage |= VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
    if (HasFlag(usageIn, BufferUsage::Uniform))
        usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (HasFlag(usageIn, BufferUsage::Storage))
        usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (HasFlag(usageIn, BufferUsage::Index))
        usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (HasFlag(usageIn, BufferUsage::Vertex))
        usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (HasFlag(usageIn, BufferUsage::Indirect))
        usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

    return usage;
}

VKBuffer::VKBuffer(const CreateInfo& createInfo)
    : Buffer(createInfo.usages), allocator(VKContext::Instance()->allocator)
{
    usage = MapBufferUsage(createInfo.usages);
    size = createInfo.size;

    if (createInfo.visibleInCPU)
    {
        vmaAllocationCreateFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }
    else
    {
        vmaAllocationCreateFlags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    }

    VkBufferCreateInfo vkCreateInfo{};
    vkCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vkCreateInfo.pNext = VK_NULL_HANDLE;
    vkCreateInfo.flags = 0;
    vkCreateInfo.size = size;
    vkCreateInfo.usage = usage;
    vkCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateInfo.queueFamilyIndexCount = 1;
    uint32_t queueFamily = VKContext::Instance()->mainQueue->queueFamilyIndex;
    vkCreateInfo.pQueueFamilyIndices = &queueFamily;

    VmaAllocationCreateInfo allocationCreateInfo{};
    allocationCreateInfo.usage = vmaMemUsage;
    allocationCreateInfo.flags = vmaAllocationCreateFlags;

    allocator->CreateBuffer(vkCreateInfo, allocationCreateInfo, buffer, allocation, &allocationInfo);

    if (createInfo.debugName)
    {
        SetDebugName(createInfo.debugName);
    }
}

VKBuffer::VKBuffer(VkBufferCreateInfo& createInfo, VmaAllocationCreateInfo& allocationCreateInfo, const char* debugName)
    : Buffer(BufferUsage::None), allocator(VKContext::Instance()->allocator), usage(createInfo.usage), allocationInfo{}
{
    allocator->CreateBuffer(createInfo, allocationCreateInfo, buffer, allocation, &allocationInfo);
    if (debugName)
    {
        SetDebugName(debugName);
    }
}

void VKBuffer::SetDebugName(const char* name)
{
    this->name = name;
    VKDebugUtils::SetDebugName(VK_OBJECT_TYPE_BUFFER, (uint64_t)buffer, name);
}

void VKBuffer::FillMemoryBarrierIfNeeded(
    std::vector<VkBufferMemoryBarrier>& barriers, VkPipelineStageFlags stageMask, VkAccessFlags accessMask
)
{
    if (this->stageMask != stageMask || this->accessMask != accessMask)
    {
        barriers.emplace_back();
        VkBufferMemoryBarrier& memoryBarrier = barriers.back();
        memoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        memoryBarrier.pNext = VK_NULL_HANDLE;
        memoryBarrier.srcAccessMask = this->accessMask;
        memoryBarrier.dstAccessMask = accessMask;
        memoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        memoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        memoryBarrier.buffer = buffer;
        memoryBarrier.offset = 0;
        memoryBarrier.size = VK_WHOLE_SIZE;

        this->stageMask = stageMask;
        this->accessMask = accessMask;
    }
}

void VKBuffer::PutMemoryBarrierIfNeeded(
    VkCommandBuffer cmdBuf, VkPipelineStageFlags stageMask, VkAccessFlags accessMask
)
{
    if (this->stageMask != stageMask || this->accessMask != accessMask)
    {
        VkBufferMemoryBarrier memoryBarrier;
        memoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        memoryBarrier.pNext = VK_NULL_HANDLE;
        memoryBarrier.srcAccessMask = this->accessMask;
        memoryBarrier.dstAccessMask = accessMask;
        memoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        memoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        memoryBarrier.buffer = buffer;
        memoryBarrier.offset = 0;
        memoryBarrier.size = VK_WHOLE_SIZE;
        vkCmdPipelineBarrier(
            cmdBuf,
            this->stageMask,
            stageMask,
            VK_DEPENDENCY_BY_REGION_BIT,
            0,
            VK_NULL_HANDLE,
            1,
            &memoryBarrier,
            0,
            VK_NULL_HANDLE
        );

        this->stageMask = stageMask;
        this->accessMask = accessMask;
    }
}

VKBuffer::~VKBuffer()
{
    if (buffer != VK_NULL_HANDLE)
        allocator->DestroyBuffer(buffer, allocation);
}

void* VKBuffer::GetCPUVisibleAddress()
{
    return allocationInfo.pMappedData;
}
} // namespace Gfx
