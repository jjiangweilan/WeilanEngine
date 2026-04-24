#pragma once
#include "Engine/Driver/GfxDriver/Vulkan/VKCommon.hpp"

namespace Gfx::VKUtils
{
bool FormatHasDepth(VkFormat format);
bool FormatHasStencil(VkFormat format);
} // namespace Gfx::VKUtils
