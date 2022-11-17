#include "VKBuffer.hpp"
#include "GfxDriver/Vulkan/VKContext.hpp"
#include "GfxDriver/Vulkan/Internal/VKDevice.hpp"
#include "GfxDriver/Vulkan/Internal/VKMemAllocator.hpp"
#include "VKDebugUtils.hpp"

// reference: https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/usage_patterns.html
namespace Engine::Gfx
{
    VkBufferUsageFlags MapBufferUsage(BufferUsage usageIn)
    {
        VkBufferUsageFlags usage = 0;
        switch(usageIn)
        {
            case BufferUsage::Index : usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT; break;
            case BufferUsage::Vertex : usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT; break;
            case BufferUsage::Uniform : usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT; break;
            case BufferUsage::Storage : usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; break;
            case BufferUsage::Unknown : assert(0 && "Buffer usage is not handled");
        }

        usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        return usage;
    }

    VKBuffer::VKBuffer(uint32_t size, VkBufferUsageFlags usage, bool readback):
        GfxBuffer(size), allocator(VKContext::Instance()->allocator), usage(usage), allocationInfo{}
    {
        if (readback)
        {
            vmaAllocationCreateFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
                VMA_ALLOCATION_CREATE_MAPPED_BIT;
        }
        else
        {
            vmaAllocationCreateFlags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        }
    }

    void VKBuffer::SetDebugName(const char* name)
    {
        this->name = name;
    }

    void VKBuffer::PutMemoryBarrier(VkCommandBuffer cmdBuf, VkPipelineStageFlags stageMask, VkAccessFlags accessMask)
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
        vkCmdPipelineBarrier(cmdBuf, this->stageMask, stageMask, VK_DEPENDENCY_BY_REGION_BIT, 0, VK_NULL_HANDLE, 1, &memoryBarrier, 0, VK_NULL_HANDLE);

        this->stageMask = stageMask;
        this->accessMask = accessMask;
    }

    VKBuffer::VKBuffer(uint32_t size, BufferUsage bu, bool readback) : 
        VKBuffer(size, MapBufferUsage(bu), readback) {}

    VKBuffer::~VKBuffer()
    {
        if (buffer != VK_NULL_HANDLE)
            allocator->DestroyBuffer(buffer, allocation);
    }

    VkBuffer VKBuffer::GetVKBuffer()
    {
        if (buffer == VK_NULL_HANDLE) CreateBuffer();
        return buffer;
    }

    void VKBuffer::Write(void* data, uint32_t dataSize, uint32_t offsetInDst)
    {
        if (buffer == VK_NULL_HANDLE)
        {
            CreateBuffer();
        }

        DataRange range;
        range.data = data;
        range.offsetInSrc = 0;
        range.size = dataSize;
        allocator->UploadBuffer(this, offsetInDst, dataSize, &range, 1);
    }

    void* VKBuffer::GetCPUVisibleAddress()
    {
        if (buffer == VK_NULL_HANDLE)
        {
            CreateBuffer();
        }

        return allocationInfo.pMappedData;
    }

    void VKBuffer::CreateBuffer()
    {
        VkBufferCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        createInfo.pNext = VK_NULL_HANDLE;
        createInfo.flags = 0;
        createInfo.size = size;
        createInfo.usage = usage;
        createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 1;
        uint32_t queueFamily = VKContext::Instance()->mainQueue->queueFamilyIndex;
        createInfo.pQueueFamilyIndices = &queueFamily;

        VmaAllocationCreateInfo allocationCreateInfo{};
        allocationCreateInfo.usage = vmaMemUsage;
        allocationCreateInfo.flags = vmaAllocationCreateFlags;

        allocator->CreateBuffer(createInfo, allocationCreateInfo, buffer, allocation, &allocationInfo);
        VKDebugUtils::SetDebugName(VK_OBJECT_TYPE_BUFFER, (uint64_t)buffer, name.c_str());
    }

    void VKBuffer::Resize(uint32_t size)
    {
        if (buffer != VK_NULL_HANDLE)
        {
            allocator->DestroyBuffer(buffer, allocation);
        }

        this->size = size;
        CreateBuffer();
    }
}
