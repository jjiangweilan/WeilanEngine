#include "VKDebugUtils.hpp"
#include "VKContext.hpp"

namespace Gfx
{
void VKDebugUtils::SetDebugName(VkObjectType type, uint64_t object, const char* name)
{
    if (vkSetDebugUtilsObjectNameEXT != nullptr)
    {
        VkDebugUtilsObjectNameInfoEXT
            nameInfo{VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT, VK_NULL_HANDLE, type, (uint64_t)object, name};

        vkSetDebugUtilsObjectNameEXT(GetDevice(), &nameInfo);
    }
}

void VKDebugUtils::CmdBeginLabel(VkCommandBuffer cmd, const char* label, float color[4])
{
    if (vkCmdBeginDebugUtilsLabelEXT != nullptr)
    {
        VkDebugUtilsLabelEXT l{
            VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
            VK_NULL_HANDLE,
            label,
            {color[0], color[1], color[2], color[3]}};

        vkCmdBeginDebugUtilsLabelEXT(cmd, &l);
    }
}

void VKDebugUtils::CmdInsertLabel(VkCommandBuffer cmd, const char* label, float color[4])
{
    if (vkCmdInsertDebugUtilsLabelEXT != nullptr)
    {
        VkDebugUtilsLabelEXT l{
            VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
            VK_NULL_HANDLE,
            label,
            {color[0], color[1], color[2], color[3]}};

        vkCmdInsertDebugUtilsLabelEXT(cmd, &l);
    }
}

void VKDebugUtils::CmdEndLabel(VkCommandBuffer cmd)
{
    if (vkCmdEndDebugUtilsLabelEXT != nullptr)
    {
        vkCmdEndDebugUtilsLabelEXT(cmd);
    }
}
} // namespace Gfx
