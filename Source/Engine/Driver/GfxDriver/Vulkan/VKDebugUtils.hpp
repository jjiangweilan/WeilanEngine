#pragma once
#include <vulkan/vulkan.h>

namespace Gfx
{
class VKDebugUtils
{
public:
    static void Init(VkInstance instance);

    static void SetDebugName(VkObjectType type, uint64_t object, const char* name);
    static void CmdBeginLabel(VkCommandBuffer cmd, const char* label, float color[4]);
    static void CmdEndLabel(VkCommandBuffer cmd);
    static void CmdInsertLabel(VkCommandBuffer cmd, const char* label, float color[4]);

private:
    static PFN_vkSetDebugUtilsObjectNameEXT SetDebugUtilsObjectName;
    static PFN_vkCmdBeginDebugUtilsLabelEXT CmdBeginDebugUtilsLabelFunc;
    static PFN_vkCmdEndDebugUtilsLabelEXT CmdEndDebugUtilsLabelFunc;
    static PFN_vkCmdInsertDebugUtilsLabelEXT CmdInsertDebugUtilsLabelFunc;
};
} // namespace Gfx
