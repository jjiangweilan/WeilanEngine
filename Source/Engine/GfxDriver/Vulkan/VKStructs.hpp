#pragma once
#include <vulkan/vulkan.h>
namespace Engine::Gfx
{
    struct DeviceQueue
    {
        VkQueue queue = VK_NULL_HANDLE;
        uint32_t queueIndex = -1;
        uint32_t queueFamilyIndex = -1;
    };

}
