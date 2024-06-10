#pragma once
#include "../Fence.hpp"
#include "VKContext.hpp"
#include <vulkan/vulkan.h>

namespace Gfx
{
class VKFence : public Fence
{
public:
    VKFence(const Fence::CreateInfo& createInfo)
    {
        VkFenceCreateInfo vkCreateInfo;
        vkCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        vkCreateInfo.pNext = VK_NULL_HANDLE;
        vkCreateInfo.flags = createInfo.signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;
        vkCreateFence(GetDevice(), &vkCreateInfo, VK_NULL_HANDLE, &vkFence);
    }

    ~VKFence()
    {
        vkDestroyFence(GetDevice(), vkFence, VK_NULL_HANDLE);
    }

    void Reset()
    {
        vkResetFences(GetDevice(), 1, &vkFence);
    };
    VkFence GetHandle() const
    {
        return vkFence;
    }

private:
    VkFence vkFence;
};
} // namespace Gfx
