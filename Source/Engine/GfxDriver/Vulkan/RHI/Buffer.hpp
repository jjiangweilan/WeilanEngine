#pragma once
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace Gfx::Vulkan
{
struct Buffer
{
    VkBuffer handle = VK_NULL_HANDLE;
    VmaAllocation allocation;
    VmaAllocationInfo allocationInfo;
    size_t size;
};
} // namespace Gfx::Vulkan
