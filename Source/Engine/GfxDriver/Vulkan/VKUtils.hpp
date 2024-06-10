#pragma once
#include <vulkan/vulkan.h>

namespace Gfx::VK
{
inline bool HasWriteAccessMask(VkAccessFlags flags)
{
    if ((flags & VK_ACCESS_SHADER_WRITE_BIT)|| (flags & VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)||
        (flags & VK_ACCESS_TRANSFER_WRITE_BIT)|| (flags & VK_ACCESS_HOST_WRITE_BIT) ||
        (flags & VK_ACCESS_MEMORY_WRITE_BIT) || (flags & VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT))
        return true;

    return false;
}

inline bool HasReadAccessMask(VkAccessFlags flags)
{
    if ((flags & VK_ACCESS_INDIRECT_COMMAND_READ_BIT) || (flags & VK_ACCESS_INDEX_READ_BIT) ||
        (flags & VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT) || (flags & VK_ACCESS_UNIFORM_READ_BIT) ||
        (flags & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT) || (flags & VK_ACCESS_SHADER_READ_BIT) ||
        (flags & VK_ACCESS_COLOR_ATTACHMENT_READ_BIT) ||
        (flags & VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT)|| (flags & VK_ACCESS_TRANSFER_READ_BIT) ||
        (flags & VK_ACCESS_HOST_READ_BIT) || (flags & VK_ACCESS_MEMORY_READ_BIT)|| (flags & VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT))
        return true;
    return false;
}
} // namespace Gfx::VK
