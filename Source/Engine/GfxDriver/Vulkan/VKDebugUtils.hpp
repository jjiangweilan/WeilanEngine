#pragma once
#include <vulkan/vulkan.h>

namespace Engine::Gfx
{
    class VKDebugUtils
    {
        public:
            static void Init(VkInstance instance);
            static PFN_vkSetDebugUtilsObjectNameEXT SetDebugUtilsObjectName;
    };
}
