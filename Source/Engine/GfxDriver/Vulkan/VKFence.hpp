#pragma once
#include "../Fence.hpp"
#include "VKContext.hpp"
#include <vulkan/vulkan.h>

namespace Engine::Gfx
{
    class VKFence : public Fence
    {
        public:
            VKFence(bool signaled)
            {
                VkFenceCreateInfo createInfo;
                createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
                createInfo.pNext = VK_NULL_HANDLE;
                createInfo.flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;
                vkCreateFence(GetDevice()->GetHandle(), &createInfo, VK_NULL_HANDLE, &vkFence);
            }

            ~VKFence()
            {
                vkDestroyFence(GetDevice()->GetHandle(), vkFence, VK_NULL_HANDLE);
            }

            VkFence GetHandle() const { return vkFence; }

        private:
            VkFence vkFence;
    };
}
