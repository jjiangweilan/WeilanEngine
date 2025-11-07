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
DEFINE_OBJECT(VKImage, "D713D759-996D-4CB1-BDF2-12ED4F0CB043");
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

    arrayLayers = imageDescription.GetLayer();

    if (imageDescription.width <= 0 || imageDescription.height <= 0)
    {
        spdlog::warn("Trying to create a zero sized image");
        return;
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

    arrayLayers = imageDescription.GetLayer();
    CreateImageView();

    SetName("Unnamed");
    layoutTrack.resize(arrayLayers * imageDescription.mipLevels, VK_IMAGE_LAYOUT_UNDEFINED);
}

VKImage::VKImage(VKImage&& other)
    : Image(other.usageFlags), arrayLayers(other.arrayLayers), imageType_vk(other.imageType_vk),
      usageFlags(other.usageFlags), image_vk(std::exchange(other.image_vk, VK_NULL_HANDLE)),
      allocation_vma(std::exchange(other.allocation_vma, VK_NULL_HANDLE)), stageMask(other.stageMask),
      accessMask(other.accessMask), imageDescription(other.imageDescription),
      imageView(std::exchange(other.imageView, VK_NULL_HANDLE)), imageViewForShaderResource(std::exchange(other.imageViewForShaderResource, VK_NULL_HANDLE)),
      layoutTrack(std::exchange(other.layoutTrack, {}))

{}

VKImage::~VKImage()
{
    imageView = nullptr;
    imageViewForShaderResource = nullptr;
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

    VKContext::Instance()->allocator->CreateImage(imageCreateInfo, image_vk, allocation_vma, &allocationInfo_vma);
}

void VKImage::CreateImageView()
{
    auto defaultSubresourceRange = GenerateDefaultSubresourceRange();
    imageView = std::unique_ptr<VKImageView>(new VKImageView({
        .image = *this,
        .imageViewType = GenerateDefaultImageViewViewType(),
        .subresourceRange = defaultSubresourceRange,
    }));
    imageView->SetName("Default ImageView");

    // create a new default image view for when aspect mask has both dept and stencil
    if (HasFlag(defaultSubresourceRange.aspectMask, ImageAspect::Depth) &&
        HasFlag(defaultSubresourceRange.aspectMask, ImageAspect::Stencil))
    {
        defaultSubresourceRange.aspectMask = ImageAspect::Depth;
        imageViewForShaderResource = std::unique_ptr<VKImageView>(new VKImageView({
            .image = *this,
            .imageViewType = GenerateDefaultImageViewViewType(),
            .subresourceRange = defaultSubresourceRange,
        }));
        imageViewForShaderResource->SetName("Default ImageView");
    }
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
    imageView->SetName(fmt::format("{}-Default ImageView", name));
}

// TODO: this needs improvement. Cube and some others are not handled
// https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VkImageViewCreateInfo.html
ImageViewType VKImage::GenerateDefaultImageViewViewType()
{
    if (imageDescription.isCubemap)
    {
        return ImageViewType::Cubemap;
    }

    ImageViewType ret = ImageViewType::Image_2D;
    switch (imageType_vk)
    {
        case VK_IMAGE_TYPE_1D: ret = ImageViewType::Image_1D; break;
        case VK_IMAGE_TYPE_2D: ret = ImageViewType::Image_2D; break;
        case VK_IMAGE_TYPE_3D: ret = ImageViewType::Image_3D; break;
        default: break;
    }

    if (imageDescription.GetLayer() > 1)
    {
        if (ret == ImageViewType::Image_2D)
            ret = ImageViewType::Image_2D_Array;
        else if (ret == ImageViewType::Image_1D)
            ret = ImageViewType::Image_1D_Array;
    }

    return ret;
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
    range.aspectMask = option.aspect;
    range.baseMipLevel = glm::clamp(option.baseMipLevel, 0, (int)imageDescription.mipLevels - 1);
    range.levelCount = glm::min(
        Gfx::Remaining_Mip_Levels ? imageDescription.mipLevels : option.levelCount,
        imageDescription.mipLevels - option.baseMipLevel);
    range.baseArrayLayer = glm::clamp(option.baseArrayLayer, 0, (int)imageDescription.layers - 1);
    range.layerCount = glm::min(
        option.layerCount == Gfx::Remaining_Array_Layers ? imageDescription.layers : option.layerCount, 
        imageDescription.layers - option.baseArrayLayer);

    ImageViewType imageViewType = ImageViewType::Image_2D;
    switch (imageType_vk)
    {
        case VK_IMAGE_TYPE_1D: imageViewType = ImageViewType::Image_1D; break;
        case VK_IMAGE_TYPE_2D: imageViewType = ImageViewType::Image_2D; break;
        case VK_IMAGE_TYPE_3D: imageViewType = ImageViewType::Image_3D; break;
        default: break;
    }

    if (range.layerCount > 1 || option.asArray)
    {
        if (imageViewType == ImageViewType::Image_2D)
            imageViewType = ImageViewType::Image_2D_Array;
        else if (imageViewType == ImageViewType::Image_1D)
            imageViewType = ImageViewType::Image_1D_Array;
    }

    ImageView::CreateInfo imageViewCreateInfo{
        .image = *this,
        .imageViewType = imageViewType,
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
        auto imageView = std::unique_ptr<VKImageView>(new VKImageView(imageViewCreateInfo));
        imageView->SetName(fmt::format("{}-mip {}-levelCount {}-baseArrayLayer {}-layerCount {}-aspectMask {}", GetName(), range.baseMipLevel, range.levelCount, range.baseArrayLayer, range.layerCount, (int)range.aspectMask));
        auto temp = imageView.get();
        imageViews[vkImageViewCreateInfo] = std::move(imageView);
        return *temp;
    }
}

ImageView& VKImage::GetDefaultImageViewForShaderResource()
{
    if (imageViewForShaderResource != nullptr)
        return *imageViewForShaderResource;

    return GetDefaultImageView();
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
