#pragma once
#include "Engine/Driver/GfxDriver/Vulkan/VKCommon.hpp"

namespace Gfx
{
class VKDebugUtils
{
public:
    static void SetDebugName(VkObjectType type, uint64_t object, const char* name);
    static void CmdBeginLabel(VkCommandBuffer cmd, const char* label, float color[4]);
    static void CmdEndLabel(VkCommandBuffer cmd);
    static void CmdInsertLabel(VkCommandBuffer cmd, const char* label, float color[4]);
};
} // namespace Gfx
