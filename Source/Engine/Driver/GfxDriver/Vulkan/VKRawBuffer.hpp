#pragma once
#include "Engine/Driver/GfxDriver/Vulkan/VKCommon.hpp"

namespace Gfx
{
struct VKRawBuffer
{
    VkBuffer handle = VK_NULL_HANDLE;
    VmaAllocation allocation;
    VmaAllocationInfo allocationInfo;
    size_t size;
};
} // namespace Gfx
