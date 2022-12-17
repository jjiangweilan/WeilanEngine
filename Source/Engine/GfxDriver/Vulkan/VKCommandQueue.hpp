#pragma once

#include <vulkan/vulkan.h>
#include "GfxDriver/CommandQueue.hpp"

namespace Engine::Gfx
{
    class VKCommandQueue : public CommandQueue
    {
        public:
            ~VKCommandQueue() override {}
            VkQueue queue = VK_NULL_HANDLE;
            uint32_t queueIndex = -1;
            uint32_t queueFamilyIndex = -1;
    };
}
