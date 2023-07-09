#pragma once
#include <vulkan/vulkan.h>

namespace Engine::Gfx
{
class VKExtensionFunc
{
public:
    static PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetKHR;
};
} // namespace Engine::Gfx
