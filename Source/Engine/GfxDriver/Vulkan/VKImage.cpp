#include "VKImage.hpp"
#include "Internal/VKEnumMapper.hpp"
#include "Internal/VKMemAllocator.hpp"
#include "Internal/VKObjectManager.hpp"
#include "Internal/VKUtils.hpp"
#include "VKContext.hpp"
#include "VKDebugUtils.hpp"
#include "VKDriver.hpp"
#include "VKImageView.hpp"
#include <vk_mem_alloc.h>

#include <spdlog/spdlog.h>

namespace Gfx
{
static bool IsGPUWrite(ImageUsageFlags usageFlags)
{
    return (usageFlags & ImageUsage::Storage) ||
           ((usageFlags & ImageUsage::ColorAttachment) | (usageFlags & ImageUsage::DepthStencilAttachment));
}

VKImage::VKImage() : Image(false), imageView(nullptr) {};
VKImage::VKImage(const ImageDescription& imageDescription, ImageUsageFlags usageFlags)
    : Image(::Gfx::IsGPUWrite(usageFlags)), usageFlags(MapImageUsage(usageFlags)), imageDescription(imageDescription),
      imageView(nullptr)
{
    format_vk = MapFormat(imageDescription.format);

    if (imageDescription.isCubemap)
    {
        arrayLayers = 6;
    }

    MakeVkObjects();
    CreateImageView();

    SetName("Unnamed");
    layoutTrack.resize(arrayLayers * imageDescription.mipLevels, VK_IMAGE_LAYOUT_UNDEFINED);
}

VKImage::VKImage(VkImage image, const ImageDescription& imageDescription, ImageUsageFlags usageFlags)
    : Image(::Gfx::IsGPUWrite(usageFlags)), usageFlags(MapImageUsage(usageFlags)), image_vk(image),
      imageDescription(imageDescription), imageView(nullptr)
{
    format_vk = MapFormat(imageDescription.format);

    if (imageDescription.isCubemap)
    {
        arrayLayers = 6;
    }
    CreateImageView();

    SetName("Unnamed");
    layoutTrack.resize(arrayLayers * imageDescription.mipLevels, VK_IMAGE_LAYOUT_UNDEFINED);
}

VKImage::VKImage(VKImage&& other)
    : Image(other.usageFlags), arrayLayers(other.arrayLayers), imageType_vk(other.imageType_vk),
      usageFlags(other.usageFlags), image_vk(std::exchange(other.image_vk, VK_NULL_HANDLE)),
      allocation_vma(std::exchange(other.allocation_vma, VK_NULL_HANDLE)), layout(other.layout),
      stageMask(other.stageMask), accessMask(other.accessMask), imageDescription(other.imageDescription),
      imageView(std::exchange(other.imageView, VK_NULL_HANDLE)), layoutTrack(std::exchange(other.layoutTrack, {}))
{}

VKImage::~VKImage()
{
    if (image_vk != VK_NULL_HANDLE && allocation_vma != nullptr)
        VKContext::Instance()->allocator->DestoryImage(image_vk, allocation_vma);
}

void VKImage::MakeVkObjects()
{
    if (imageDescription.depth > 1)
    {
        imageType_vk = VK_IMAGE_TYPE_3D;
    }

    // create the image
    VkImageCreateInfo imageCreateInfo;
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.pNext = VK_NULL_HANDLE;
    imageCreateInfo.flags = imageDescription.isCubemap ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
    imageCreateInfo.imageType = imageType_vk;
    imageCreateInfo.format = format_vk;
    imageCreateInfo.extent = {imageDescription.width, imageDescription.height, imageDescription.depth};
    imageCreateInfo.mipLevels = imageDescription.mipLevels;
    imageCreateInfo.arrayLayers = arrayLayers;
    imageCreateInfo.samples = MapSampleCount(imageDescription.multiSampling);
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage = usageFlags;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.queueFamilyIndexCount = 1;
    imageCreateInfo.pQueueFamilyIndices = &VKContext::Instance()->mainQueue->queueFamilyIndex;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    layout = VK_IMAGE_LAYOUT_UNDEFINED;

    VKContext::Instance()->allocator->CreateImage(imageCreateInfo, image_vk, allocation_vma, &allocationInfo_vma);
}

void VKImage::CreateImageView()
{
    imageView = std::unique_ptr<VKImageView>(new VKImageView({
        .image = *this,
        .imageViewType = GenerateDefaultImageViewViewType(),
        .subresourceRange = GenerateDefaultSubresourceRange(),
    }));
}

ImageSubresourceRange VKImage::GetSubresourceRange()
{
    ImageSubresourceRange range;
    range.aspectMask = ImageAspect::None;

    bool hasDepth = VKUtils::FormatHasDepth(format_vk);
    bool hasStencil = VKUtils::FormatHasStencil(format_vk);
    if (hasDepth)
        range.aspectMask |= ImageAspect::Depth;
    if (hasStencil)
        range.aspectMask |= ImageAspect::Stencil;
    if (!hasDepth && !hasStencil)
        range.aspectMask |= ImageAspect::Color;

    range.baseMipLevel = 0;
    range.levelCount = imageDescription.mipLevels;
    range.baseArrayLayer = 0;
    range.layerCount = arrayLayers;

    return range;
}

ImageSubresourceRange VKImage::GenerateDefaultSubresourceRange()
{
    VkFormat format_vk = MapFormat(imageDescription.format);
    ImageSubresourceRange range;

    range.aspectMask = Gfx::ImageAspect::None;
    bool hasDepth = VKUtils::FormatHasDepth(format_vk);
    bool hasStencil = VKUtils::FormatHasStencil(format_vk);
    if (hasDepth)
        range.aspectMask |= Gfx::ImageAspect::Depth;
    if (hasStencil)
        range.aspectMask |= Gfx::ImageAspect::Stencil;
    if (!hasDepth && !hasStencil)
        range.aspectMask |= Gfx::ImageAspect::Color;

    range.baseMipLevel = 0;
    range.levelCount = imageDescription.mipLevels;
    range.baseArrayLayer = 0;
    range.layerCount = arrayLayers;

    return range;
}

void VKImage::SetName(std::string_view name)
{
    this->name = name;

    VKDebugUtils::SetDebugName(VK_OBJECT_TYPE_IMAGE, (uint64_t)image_vk, this->name.c_str());
}

// TODO: this needs improvement. Cube and some others are not handled
// https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VkImageViewCreateInfo.html
ImageViewType VKImage::GenerateDefaultImageViewViewType()
{
    if (imageDescription.isCubemap)
    {
        return ImageViewType::Cubemap;
    }

    switch (imageType_vk)
    {
        case VK_IMAGE_TYPE_1D: return ImageViewType::Image_1D;
        case VK_IMAGE_TYPE_2D: return ImageViewType::Image_2D;
        case VK_IMAGE_TYPE_3D: return ImageViewType::Image_3D;
        default: break;
    }

    return ImageViewType::Image_2D;
}

ImageView& VKImage::GetDefaultImageView()
{
    return *imageView;
}

VkImageView VKImage::GetDefaultVkImageView()
{
    return imageView->GetHandle();
}

VkImageSubresourceRange VKImage::GetDefaultSubresourceRange()
{
    return imageView->GetVkSubresourceRange();
}

ImageLayout VKImage::GetImageLayout()
{
    return MapVKImageLayout(layout);
}

void VKImage::SetData(std::span<uint8_t> binaryData, uint32_t mip, uint32_t layer)
{
    SetData(binaryData, mip, layer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void VKImage::SetData(std::span<uint8_t> binaryData, uint32_t mip, uint32_t layer, VkImageLayout finalLayout)
{
    GetDriver()->UploadImage(
        *this,
        binaryData.data(),
        binaryData.size(),
        mip,
        layer,
        GetSubresourceRange().aspectMask,
        finalLayout
    );
}

VkImageSubresourceRange MapVkImageSubresourceRange(const ImageSubresourceRange& r)
{
    VkImageSubresourceRange range;
    range.aspectMask = MapImageAspect(r.aspectMask);
    range.baseMipLevel = r.baseMipLevel;
    range.levelCount = r.levelCount;
    range.baseArrayLayer = r.baseArrayLayer;
    range.layerCount = r.layerCount;
    return range;
}

ImageView& VKImage::GetImageView(const ImageViewOption& option)
{
    ImageSubresourceRange range = GenerateDefaultSubresourceRange();
    range.baseMipLevel = option.baseMipLevel;
    range.levelCount = option.levelCount;
    range.baseArrayLayer = option.baseArrayLayer;
    range.layerCount = option.layerCount;

    ImageView::CreateInfo imageViewCreateInfo{
        .image = *this,
        .imageViewType = GenerateDefaultImageViewViewType(),
        .subresourceRange = range,
    };

    auto vkImageViewCreateInfo = MapImageViewCreateInfo(this, imageViewCreateInfo);
    auto imageViewIter = imageViews.find(vkImageViewCreateInfo);
    if (imageViewIter != imageViews.end())
    {
        return *imageViewIter->second;
    }
    else
    {
        imageView = std::unique_ptr<VKImageView>(new VKImageView(imageViewCreateInfo));
        auto temp = imageView.get();
        imageViews[vkImageViewCreateInfo] = std::move(imageView);
        return *temp;
    }
}

// VKSwapChainImageProxy::~VKSwapChainImageProxy() {}
//
// VKSwapChainImageProxy::VKSwapChainImageProxy(){};
//
// void VKSwapChainImageProxy::SetActiveSwapChainImage(RefPtr<VKImage> activeImage, uint32_t index)
// {
//     this->activeImage = activeImage;
//     this->activeIndex = index;
//     static_cast<VKImageView&>(this->activeImage->GetDefaultImageView()).ChangeOwner(this);
//
//     imageDescription = activeImage->GetDescription();
// };
} // namespace Gfx
