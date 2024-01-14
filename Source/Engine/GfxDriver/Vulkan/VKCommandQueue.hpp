#pragma once

#include "GfxDriver/CommandQueue.hpp"
#include <vulkan/vulkan.h>

namespace Gfx
{
class VKCommandQueue : public CommandQueue
{
public:
    ~VKCommandQueue() override {}
    virtual uint32_t GetFamilyIndex() override
    {
        return queueFamilyIndex;
    }
    VkQueue handle = VK_NULL_HANDLE;
    uint32_t queueIndex = -1;
    uint32_t queueFamilyIndex = -1;
};
} // namespace Gfx
