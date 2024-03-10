#include "VKImageView.hpp"
#include "GfxDriver/Vulkan/Internal/VKEnumMapper.hpp"
#include "VKContext.hpp"

namespace Gfx
{
VkImageViewType MapImageViewType(ImageViewType type)
{
    switch (type)
    {
        case ImageViewType::Image_1D: return VK_IMAGE_VIEW_TYPE_1D;
        case ImageViewType::Image_2D: return VK_IMAGE_VIEW_TYPE_2D;
        case ImageViewType::Image_3D: return VK_IMAGE_VIEW_TYPE_3D;
        case ImageViewType::Cubemap: return VK_IMAGE_VIEW_TYPE_CUBE;
        case ImageViewType::Image_1D_Array: return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
        case ImageViewType::Image_2D_Array: return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        case ImageViewType::Cube_Array: return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
    }
}

VKImageView::VKImageView(const CreateInfo& createInfo)
    : image(static_cast<VKImage*>(&createInfo.image)), subresourceRange(createInfo.subresourceRange),
      imageViewType(createInfo.imageViewType)
{
    VkImageViewCreateInfo imageViewCreateInfo;
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.pNext = VK_NULL_HANDLE;
    imageViewCreateInfo.flags = 0;
    imageViewCreateInfo.image = image->GetImage();
    imageViewCreateInfo.viewType = MapImageViewType(createInfo.imageViewType);
    imageViewCreateInfo.format = MapFormat(image->GetDescription().format);
    imageViewCreateInfo.components = {
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY};

    VkImageSubresourceRange subresourceRange{
        .aspectMask = MapImageAspect(createInfo.subresourceRange.aspectMask),
        .baseMipLevel = createInfo.subresourceRange.baseMipLevel,
        .levelCount = createInfo.subresourceRange.levelCount,
        .baseArrayLayer = createInfo.subresourceRange.baseArrayLayer,
        .layerCount = createInfo.subresourceRange.layerCount,
    };

    imageViewCreateInfo.subresourceRange = subresourceRange;

    VKContext::Instance()->objManager->CreateImageView(imageViewCreateInfo, handle);
}

VKImageView::VKImageView(VKImageView&& other)
    : image(std::exchange(other.image, nullptr)), handle(std::exchange(other.handle, VK_NULL_HANDLE)),
      subresourceRange(other.subresourceRange), imageViewType(other.imageViewType){};

VkImageSubresourceRange VKImageView::GetVkSubresourceRange()
{
    return {
        .aspectMask = MapImageAspect(subresourceRange.aspectMask),
        .baseMipLevel = subresourceRange.baseMipLevel,
        .levelCount = subresourceRange.levelCount,
        .baseArrayLayer = subresourceRange.baseArrayLayer,
        .layerCount = subresourceRange.layerCount,
    };
}

VKImageView::~VKImageView()
{
    if (handle != VK_NULL_HANDLE)
        VKContext::Instance()->objManager->DestroyImageView(handle);
}

} // namespace Gfx
