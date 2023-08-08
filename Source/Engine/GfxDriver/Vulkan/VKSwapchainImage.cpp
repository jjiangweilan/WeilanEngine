#include "VKSwapchainImage.hpp"
#include "Internal/VKEnumMapper.hpp"

namespace Engine::Gfx
{
VKSwapChainImage::VKSwapChainImage(VkImage image, VkFormat format, uint32_t width, uint32_t height) : VKImage()
{
    this->image_vk = image;
    imageDescription.format = MapVKFormat(format);
    imageDescription.width = width;
    imageDescription.height = height;
    imageDescription.mipLevels = 1;
    format_vk = format;
    layout = VK_IMAGE_LAYOUT_UNDEFINED;
    CreateImageView();
    defaultSubResourceRange = GetDefaultSubresourceRange();
}

VKSwapChainImage::~VKSwapChainImage()
{
    image_vk = VK_NULL_HANDLE; // avoid swaph chain image being deleted by VKImage
}
} // namespace Engine::Gfx
