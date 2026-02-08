#pragma once
#include <vulkan/vulkan.h>

#if WIN32
#include <windows.h>

#include <vulkan/vulkan_win32.h>
#endif

namespace Gfx
{
class VKExtensionFunc
{
public:
    static PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetKHR;
    static PFN_vkGetMemoryWin32HandlePropertiesKHR vkGetMemoryWin32HandlePropertiesKHR;
};
} // namespace Gfx
