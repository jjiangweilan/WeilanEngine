#pragma once
#include <vulkan/vulkan.h>

namespace Gfx
{
class VKInflightCmd
{
public:
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkFence cmdFence = VK_NULL_HANDLE;
    VkSemaphore imageAcquireSemaphore = VK_NULL_HANDLE;
    VkSemaphore presentSemaphore = VK_NULL_HANDLE;
    uint32_t swapchainIndex = 0;

private:
};
} // namespace Gfx
