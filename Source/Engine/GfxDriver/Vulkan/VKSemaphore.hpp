#pragma once
#include "../Semaphore.hpp"
#include "VKContext.hpp"
#include "VKDebugUtils.hpp"
#include <vulkan/vulkan.h>

namespace Engine::Gfx
{
class VKSemaphore : public Semaphore
{
public:
    VKSemaphore(bool signaled)
    {
        VkSemaphoreCreateInfo createInfo;
        createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        createInfo.pNext = VK_NULL_HANDLE;
        createInfo.flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;

        vkCreateSemaphore(GetDevice()->GetHandle(), &createInfo, VK_NULL_HANDLE, &vkSemaphore);
    }
    ~VKSemaphore() override { vkDestroySemaphore(GetDevice()->GetHandle(), vkSemaphore, VK_NULL_HANDLE); };

    void SetName(std::string_view name) override
    {
        VKDebugUtils::SetDebugName(VK_OBJECT_TYPE_SEMAPHORE, (uint64_t)vkSemaphore, name.data());
    };

    inline VkSemaphore GetHandle() { return vkSemaphore; }

private:
    VkSemaphore vkSemaphore;
};
} // namespace Engine::Gfx
