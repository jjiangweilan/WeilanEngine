#pragma once
#include <vulkan/vulkan.h>

namespace Gfx::VKUtils
{
bool FormatHasDepth(VkFormat format);
bool FormatHasStencil(VkFormat format);
} // namespace Gfx::VKUtils
