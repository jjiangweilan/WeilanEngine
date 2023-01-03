#include "VKMemAllocator.hpp"
#include "../VKBuffer.hpp"
#include "../VKImage.hpp"
#include <cassert>
#include <spdlog/spdlog.h>
#define VK_CHECK(x)                                                                                                    \
    auto rlt_VK_CHECK = x;                                                                                             \
    assert(rlt_VK_CHECK == VK_SUCCESS);

namespace Engine::Gfx
{
VKMemAllocator::VKMemAllocator(VkInstance instance, RefPtr<VKDevice> device, VkPhysicalDevice physicalDevice,
                               uint32_t transferQueueIndex)
    : device(device), queueFamilyIndex(transferQueueIndex)
{
    // vma
    VmaAllocatorCreateInfo vmaAllocatorCreateInfo{};
    vmaAllocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_0;
    vmaAllocatorCreateInfo.physicalDevice = physicalDevice;
    vmaAllocatorCreateInfo.device = device->GetHandle();
    vmaAllocatorCreateInfo.instance = instance;

    VK_CHECK(vmaCreateAllocator(&vmaAllocatorCreateInfo, &allocator_vma));
}

VKMemAllocator::~VKMemAllocator() { vmaDestroyAllocator(allocator_vma); }

void VKMemAllocator::CreateBuffer(VkBufferCreateInfo& bufferCreateInfo, VmaAllocationCreateInfo& allocationCreateInfo,
                                  VkBuffer& buffer, VmaAllocation& allocation, VmaAllocationInfo* allocationInfo)
{
    VK_CHECK(
        vmaCreateBuffer(allocator_vma, &bufferCreateInfo, &allocationCreateInfo, &buffer, &allocation, allocationInfo));
}

void VKMemAllocator::CreateBuffer(VkBufferCreateInfo& bufferCreateInfo, VkBuffer& buffer, VmaAllocation& allocation,
                                  VmaAllocationInfo* allocationInfo)
{
    VmaAllocationCreateInfo allocationCreateInfo{};
    allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

    VK_CHECK(
        vmaCreateBuffer(allocator_vma, &bufferCreateInfo, &allocationCreateInfo, &buffer, &allocation, allocationInfo));
}

void VKMemAllocator::CreateImage(VkImageCreateInfo& imageCreateInfo, VkImage& image, VmaAllocation& allocation,
                                 VmaAllocationInfo* allocationInfo)
{
    VmaAllocationCreateInfo allocationCreateInfo{};
    allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

    VK_CHECK(
        vmaCreateImage(allocator_vma, &imageCreateInfo, &allocationCreateInfo, &image, &allocation, allocationInfo));
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
    VK_CHECK(vmaCreateBuffer(allocator_vma,
                             &bufferCreateInfo,
                             &allocationCreateInfo,
                             &srcBuf,
                             &allocation,
                             &allocationInfo));
    return srcBuf;
}

void VKMemAllocator::DestroyBuffer(VkBuffer buffer, VmaAllocation allocation)
{
    vmaDestroyBuffer(allocator_vma, buffer, allocation);
}

void VKMemAllocator::DestoryImage(VkImage image, VmaAllocation allocation)
{
    vmaDestroyImage(allocator_vma, image, allocation);
}

} // namespace Engine::Gfx
