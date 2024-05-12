#include "VKDebugUtils.hpp"
#include "VKContext.hpp"

namespace Gfx
{

PFN_vkSetDebugUtilsObjectNameEXT VKDebugUtils::SetDebugUtilsObjectName = nullptr;
PFN_vkCmdBeginDebugUtilsLabelEXT VKDebugUtils::CmdBeginDebugUtilsLabelFunc = nullptr;
PFN_vkCmdEndDebugUtilsLabelEXT VKDebugUtils::CmdEndDebugUtilsLabelFunc = nullptr;
PFN_vkCmdInsertDebugUtilsLabelEXT VKDebugUtils::CmdInsertDebugUtilsLabelFunc = nullptr;

void VKDebugUtils::SetDebugName(VkObjectType type, uint64_t object, const char* name)
{
    VkDebugUtilsObjectNameInfoEXT
        nameInfo{VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT, VK_NULL_HANDLE, type, (uint64_t)object, name};

    SetDebugUtilsObjectName(GetDevice(), &nameInfo);
}

void VKDebugUtils::CmdBeginLabel(VkCommandBuffer cmd, const char* label, float color[4])
{
    VkDebugUtilsLabelEXT l{
        VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
        VK_NULL_HANDLE,
        label,
        {color[0], color[1], color[2], color[3]}};

    CmdBeginDebugUtilsLabelFunc(cmd, &l);
}

void VKDebugUtils::CmdInsertLabel(VkCommandBuffer cmd, const char* label, float color[4])
{
    VkDebugUtilsLabelEXT l{
        VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
        VK_NULL_HANDLE,
        label,
        {color[0], color[1], color[2], color[3]}};

    CmdInsertDebugUtilsLabelFunc(cmd, &l);
}

void VKDebugUtils::CmdEndLabel(VkCommandBuffer cmd)
{
    CmdEndDebugUtilsLabelFunc(cmd);
}

void VKDebugUtils::Init(VkInstance instance)
{
    SetDebugUtilsObjectName =
        (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT");
    CmdBeginDebugUtilsLabelFunc =
        (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr(instance, "vkCmdBeginDebugUtilsLabelEXT");
    CmdEndDebugUtilsLabelFunc =
        (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetInstanceProcAddr(instance, "vkCmdEndDebugUtilsLabelEXT");
    CmdInsertDebugUtilsLabelFunc =
        (PFN_vkCmdInsertDebugUtilsLabelEXT)vkGetInstanceProcAddr(instance, "vmCmdInsertDebugUtilsLabel");
}
} // namespace Gfx
