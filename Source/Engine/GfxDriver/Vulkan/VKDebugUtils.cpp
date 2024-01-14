#include "VKDebugUtils.hpp"
#include "VKContext.hpp"

namespace Gfx
{

PFN_vkSetDebugUtilsObjectNameEXT VKDebugUtils::SetDebugUtilsObjectName = nullptr;

void VKDebugUtils::SetDebugName(VkObjectType type, uint64_t object, const char* name)
{

    VkDebugUtilsObjectNameInfoEXT
        nameInfo{VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT, VK_NULL_HANDLE, type, (uint64_t)object, name};

    SetDebugUtilsObjectName(GetDevice(), &nameInfo);
}

void VKDebugUtils::Init(VkInstance instance)
{
    SetDebugUtilsObjectName =
        (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT");
}
} // namespace Gfx
