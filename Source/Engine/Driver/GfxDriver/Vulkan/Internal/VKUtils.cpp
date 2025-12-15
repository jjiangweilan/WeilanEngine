#include "VKUtils.hpp"

namespace Gfx::VKUtils
{
bool FormatHasDepth(VkFormat format)
{
    switch (format)
    {
        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_D16_UNORM_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
        case VK_FORMAT_D32_SFLOAT: return true;
        default: return false;
    }
}
bool FormatHasStencil(VkFormat format)
{
    switch (format)
    {
        case VK_FORMAT_D16_UNORM_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT: return true;
        default: return false;
    }
}

} // namespace Gfx::VKUtils
