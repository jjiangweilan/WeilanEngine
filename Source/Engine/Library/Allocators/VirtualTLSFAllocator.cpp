#include "VirtualTLSFAllocator.hpp"
#include <vk_mem_alloc.h>
#include <utility>

VirtualTLSFAllocator::VirtualTLSFAllocator(uint64_t totalSize) : totalSize(totalSize)
{
    VmaVirtualBlockCreateInfo createInfo = {};
    createInfo.size = totalSize;
    vmaCreateVirtualBlock(&createInfo, reinterpret_cast<VmaVirtualBlock*>(&block));
}

VirtualTLSFAllocator::~VirtualTLSFAllocator()
{
    if (block)
    {
        vmaDestroyVirtualBlock(reinterpret_cast<VmaVirtualBlock>(block));
    }
}

VirtualTLSFAllocator::VirtualTLSFAllocator(VirtualTLSFAllocator&& other) noexcept
    : block(other.block), totalSize(other.totalSize)
{
    other.block = nullptr;
    other.totalSize = 0;
}

VirtualTLSFAllocator& VirtualTLSFAllocator::operator=(VirtualTLSFAllocator&& other) noexcept
{
    if (this != &other)
    {
        if (block)
        {
            vmaDestroyVirtualBlock(reinterpret_cast<VmaVirtualBlock>(block));
        }
        block = other.block;
        totalSize = other.totalSize;
        other.block = nullptr;
        other.totalSize = 0;
    }
    return *this;
}

bool VirtualTLSFAllocator::Allocate(uint64_t size, uint32_t alignment, Allocation& outAllocation)
{
    if (block == nullptr)
        return false;

    VmaVirtualAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.size = size;
    allocCreateInfo.alignment = alignment;

    VmaVirtualAllocation vmaAlloc;
    uint64_t offset;
    VkResult res = vmaVirtualAllocate(reinterpret_cast<VmaVirtualBlock>(block), &allocCreateInfo, &vmaAlloc, &offset);

    if (res == VK_SUCCESS)
    {
        outAllocation.offset = offset;
        outAllocation.size = size;
        outAllocation.internalHandle = reinterpret_cast<void*>(vmaAlloc);
        return true;
    }

    return false;
}

void VirtualTLSFAllocator::Free(Allocation& allocation)
{
    if (block && allocation.IsValid())
    {
        vmaVirtualFree(reinterpret_cast<VmaVirtualBlock>(block), reinterpret_cast<VmaVirtualAllocation>(allocation.internalHandle));
        allocation = {};
    }
}

void VirtualTLSFAllocator::Clear()
{
    if (block)
    {
        vmaClearVirtualBlock(reinterpret_cast<VmaVirtualBlock>(block));
    }
}
