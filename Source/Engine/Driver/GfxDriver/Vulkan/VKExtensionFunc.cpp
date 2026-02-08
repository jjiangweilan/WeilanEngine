#include "VKExtensionFunc.hpp"

namespace Gfx
{
PFN_vkCmdPushDescriptorSetKHR VKExtensionFunc::vkCmdPushDescriptorSetKHR = nullptr;
PFN_vkGetMemoryWin32HandlePropertiesKHR VKExtensionFunc::vkGetMemoryWin32HandlePropertiesKHR = nullptr;
} // namespace Gfx 
