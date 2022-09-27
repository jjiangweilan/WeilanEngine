#include "VKSwapChainImage.hpp"
#include "Internal/VKEnumMapper.hpp"
#include "VKContext.hpp"
namespace Engine::Gfx
{
    VKSwapChainImage::VKSwapChainImage(RefPtr<VKContext> context, VkImage image, VkFormat format, uint32_t width, uint32_t height) : VKImage()
    {
        this->context = context;
        this->image_vk = image;
        imageDescription.format = VKEnumMapper::MapVKFormat(format);
        imageDescription.width = width;
        imageDescription.height = height;
        format_vk = format;
        layout = VK_IMAGE_LAYOUT_UNDEFINED;
        CreateImageView();
        defaultSubResourceRange = GenerateDefaultSubresourceRange();
    }

    VKSwapChainImage::~VKSwapChainImage()
    {
        image_vk = VK_NULL_HANDLE; // avoid swaph chain image being deleted by VKImage
    }
}