#pragma once
#include <vulkan/vulkan.h>

namespace Engine::Gfx::VKUtils
{
bool FormatHasDepth(VkFormat format);
bool FormatHasStencil(VkFormat format);
} // namespace Engine::Gfx::VKUtils
