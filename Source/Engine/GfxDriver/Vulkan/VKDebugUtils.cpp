#include "VKDebugUtils.hpp"

namespace Engine::Gfx
{

    PFN_vkSetDebugUtilsObjectNameEXT VKDebugUtils::SetDebugUtilsObjectName = nullptr;
    void VKDebugUtils::Init(VkInstance instance)
    {
        SetDebugUtilsObjectName = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT");
    }
}
