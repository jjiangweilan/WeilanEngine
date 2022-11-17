#pragma once
#include <vulkan/vulkan.h>

namespace Engine::Gfx
{
    class VKDebugUtils
    {
        public:
            static void Init(VkInstance instance);

            static void SetDebugName(VkObjectType type, uint64_t object, const char* name);
        private:
            static PFN_vkSetDebugUtilsObjectNameEXT SetDebugUtilsObjectName;
    };
}
