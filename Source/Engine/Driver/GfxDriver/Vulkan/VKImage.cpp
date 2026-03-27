#include "VKImage.hpp"
#include "Internal/VKEnumMapper.hpp"
#include "Internal/VKMemAllocator.hpp"
#include "Internal/VKObjectManager.hpp"
#include "Internal/VKUtils.hpp"
#include "VKContext.hpp"
#include "VKDebugUtils.hpp"
#include "VKDriver.hpp"
#include "VKImageView.hpp"

#include <spdlog/spdlog.h>

namespace Gfx
{
DEFINE_OBJECT(Object, VKImage, "D713D759-996D-4CB1-BDF2-12ED4F0CB043");
static bool IsGPUWrite(ImageUsageFlags usageFlags)
{
    return (usageFlags & ImageUsage::Storage) ||
           ((usageFlags & ImageUsage::ColorAttachment) | (usageFlags & ImageUsage::DepthStencilAttachment));
}

VKImage::VKImage() : Image(false), imageView(nullptr)
{
    InitBarrierTrack();
};
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
    InitBarrierTrack();

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
    InitBarrierTrack();

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
{
    InitBarrierTrack();
}

VKImage::~VKImage()
{
    imageView = nullptr;
    imageViewForShaderResource = nullptr;
    if (image_vk != VK_NULL_HANDLE && allocation_vma != nullptr)
        VKContext::Instance()->allocator->DestroyImage(image_vk, allocation_vma);

    // spdlog::info("VKImage is destoryed {}", GetName());
}

void VKImage::InitBarrierTrack()
{
    BarrierTrack defaultVal =
        {
            .srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
            .srcAccessMask = VK_ACCESS_2_NONE,
            .dstStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
            .dstAccessMask = VK_ACCESS_2_NONE,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };
    subresourceBarrierTrack.resize(arrayLayers * imageDescription.mipLevels, defaultVal);
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
        .image = this,
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
            .image = this,
            .imageViewType = GenerateDefaultImageViewViewType(),
            .subresourceRange = defaultSubresourceRange,
        }));
        imageViewForShaderResource->SetName("Depth Default ImageView");
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
        option.levelCount == Gfx::Remaining_Mip_Levels ? imageDescription.mipLevels - range.baseMipLevel : option.levelCount,
        imageDescription.mipLevels - range.baseMipLevel
    );
    range.baseArrayLayer = glm::clamp(option.baseArrayLayer, 0, (int)imageDescription.layers - 1);
    range.layerCount = glm::min(
        option.layerCount == Gfx::Remaining_Array_Layers ? imageDescription.layers - range.baseArrayLayer : option.layerCount,
        imageDescription.layers - range.baseArrayLayer
    );

    if (range.baseMipLevel >= imageDescription.mipLevels)
    {
        int i = 0;
    }

    ImageViewType imageViewType = ImageViewType::Image_2D;
    switch (imageType_vk)
    {
        case VK_IMAGE_TYPE_1D: imageViewType = ImageViewType::Image_1D; break;
        case VK_IMAGE_TYPE_2D: imageViewType = ImageViewType::Image_2D; break;
        case VK_IMAGE_TYPE_3D: imageViewType = ImageViewType::Image_3D; break;
        default: break;
    }

    if (range.layerCount > 1 || option.type == Gfx::ImageViewOption::Type::Array)
    {
        if (imageViewType == ImageViewType::Image_2D)
            imageViewType = ImageViewType::Image_2D_Array;
        else if (imageViewType == ImageViewType::Image_1D)
            imageViewType = ImageViewType::Image_1D_Array;
    }
    else if (option.type == Gfx::ImageViewOption::Type::Cubemap)
    {
        imageViewType = ImageViewType::Cubemap;
    }

    ImageView::CreateInfo imageViewCreateInfo{
        .image = this,
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

VkImage VKImage::GetImage()
{
    return image_vk;
}

const ImageDescription& VKImage::GetDescription()
{
    return imageDescription;
}

const std::string& VKImage::GetName() const
{
    return name;
}

bool VKImage::IsSwapchainProxy()
{
    return isSwapchainProxy;
}

void VKImage::SetLayout(VkImageSubresourceRange subresourceRange, VkImageLayout layout)
{
    for (int level = subresourceRange.baseArrayLayer;
         level < (subresourceRange.baseArrayLayer + subresourceRange.layerCount) && level < arrayLayers;
         ++level)
    {
        for (int mip = subresourceRange.baseMipLevel;
             mip < (subresourceRange.baseMipLevel + subresourceRange.levelCount) && mip < imageDescription.mipLevels;
             mip++)
        {
            layoutTrack[level * imageDescription.mipLevels + mip] = layout;
        }
    }
}

bool VKImage::IsLayout(VkImageSubresourceRange subresourceRange, VkImageLayout layout)
{
    for (int level = subresourceRange.baseArrayLayer;
         level < (subresourceRange.baseArrayLayer + subresourceRange.layerCount);
         ++level)
    {
        for (int mip = subresourceRange.baseMipLevel;
             mip < (subresourceRange.baseMipLevel + subresourceRange.levelCount);
             mip++)
        {
            if (layoutTrack[level * imageDescription.mipLevels + mip] != layout)
                return false;
        }
    }
    return true;
}

bool VKImage::QueryLayout(VkImageSubresourceRange subresourceRange, VkImageLayout& layout)
{
    layout =
        layoutTrack[subresourceRange.baseArrayLayer * imageDescription.mipLevels + subresourceRange.baseMipLevel];

    return IsLayout(subresourceRange, layout);
}

void VKImage::SafeMipRange(uint32_t& baseMipLevel, uint32_t& levelCount)
{
}

std::vector<VkImageMemoryBarrier2> VKImage::MakeBarrierIfNeeded(VkPipelineStageFlags2 stageFlags, VkAccessFlags2 accessFlags, VkImageLayout expectedImageLayout, VkImageSubresourceRange subresourceRange)
{

    std::vector<VkImageMemoryBarrier2> ret{};

    if (IsFullRange(subresourceRange))
    {
        // TODO: do full range track until separate track is needed
        return {};
    }

    const int exclusiveLastMipIdx = subresourceRange.baseMipLevel + subresourceRange.levelCount;
    const int exclusiveLastLayerIdx = subresourceRange.baseArrayLayer + subresourceRange.layerCount;
    for (int layerIdx = subresourceRange.baseArrayLayer; layerIdx < exclusiveLastLayerIdx; ++layerIdx)
    {
        for (int mipIdx = subresourceRange.baseMipLevel; mipIdx < exclusiveLastMipIdx; ++mipIdx)
        {
            bool makeBarrier = false;
            BarrierTrack& subresourceBarrier = GetBarrierTrack(layerIdx, mipIdx);
            VkPipelineStageFlags2 finalSrcStageFlags = VK_PIPELINE_STAGE_2_NONE;
            VkAccessFlags2 finalSrcAccessFlags = VK_ACCESS_NONE;

            if (HasWriteAccessMask(accessFlags) || HasWriteAccessMask(subresourceBarrier.dstAccessMask) || subresourceBarrier.newLayout != expectedImageLayout)
            {
                finalSrcStageFlags = subresourceBarrier.dstStageMask;
                finalSrcAccessFlags = subresourceBarrier.dstAccessMask;
                makeBarrier = true;
            }
            else if (HasWriteAccessMask(accessFlags) || HasWriteAccessMask(subresourceBarrier.srcAccessMask))
            {
                finalSrcStageFlags = subresourceBarrier.srcStageMask;
                finalSrcAccessFlags = subresourceBarrier.srcAccessMask;
                makeBarrier = true;
            }

            if (makeBarrier)
            {
                VkImageMemoryBarrier2 barrier{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                    .pNext = nullptr,
                    .srcStageMask = finalSrcStageFlags,
                    .srcAccessMask = finalSrcAccessFlags,
                    .dstStageMask = stageFlags,
                    .dstAccessMask = accessFlags,
                    .oldLayout = subresourceBarrier.newLayout,
                    .newLayout = expectedImageLayout,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .image = image_vk,
                    .subresourceRange = VkImageSubresourceRange{subresourceRange.aspectMask, (uint32_t)mipIdx, 1, (uint32_t)layerIdx, 1}
                };

                bool mergedWithPreviousBarrier = false;
                if (!ret.empty())
                {
                    auto& previousBarrier = ret.back();

                    // try merge with previous mip barrier
                    if (previousBarrier.oldLayout == barrier.oldLayout &&
                        previousBarrier.newLayout == barrier.newLayout &&
                        previousBarrier.subresourceRange.baseMipLevel + previousBarrier.subresourceRange.levelCount == barrier.subresourceRange.baseMipLevel &&
                        previousBarrier.subresourceRange.baseArrayLayer + previousBarrier.subresourceRange.layerCount - 1 == barrier.subresourceRange.baseArrayLayer)
                    {
                        previousBarrier.srcStageMask |= finalSrcStageFlags;
                        previousBarrier.srcAccessMask |= finalSrcAccessFlags;
                        previousBarrier.subresourceRange.levelCount += 1;
                        mergedWithPreviousBarrier = true;
                    }
                }

                if (!mergedWithPreviousBarrier)
                {
                    ret.push_back(barrier);
                }

                subresourceBarrier.srcStageMask = barrier.srcStageMask;
                subresourceBarrier.srcAccessMask = barrier.srcAccessMask;
                subresourceBarrier.dstStageMask = barrier.dstStageMask;
                subresourceBarrier.dstAccessMask = barrier.dstAccessMask;
                subresourceBarrier.oldLayout = barrier.oldLayout;
                subresourceBarrier.newLayout = barrier.newLayout;
            }
        }

        // try merge layer barrier
        if (ret.size() > 1)
        {
            const auto& rFirstBarrier = ret.at(ret.size() - 1);
            const auto& rSecondBarrier = ret.at(ret.size() - 2);

            if (
                rFirstBarrier.srcStageMask == rSecondBarrier.srcStageMask &&
                rFirstBarrier.srcAccessMask == rSecondBarrier.srcAccessMask &&
                rFirstBarrier.dstStageMask == rSecondBarrier.dstStageMask &&
                rFirstBarrier.dstAccessMask == rSecondBarrier.dstAccessMask &&
                rFirstBarrier.oldLayout == rSecondBarrier.oldLayout &&
                rFirstBarrier.newLayout == rSecondBarrier.newLayout &&
                rFirstBarrier.subresourceRange.baseMipLevel == rSecondBarrier.subresourceRange.baseMipLevel &&
                rFirstBarrier.subresourceRange.levelCount == rSecondBarrier.subresourceRange.levelCount &&
                rFirstBarrier.subresourceRange.baseArrayLayer ==
                    rSecondBarrier.subresourceRange.baseArrayLayer + rSecondBarrier.subresourceRange.layerCount &&
                rFirstBarrier.subresourceRange.layerCount == 1
            )
            {
                ret.pop_back();
                ret.back().subresourceRange.layerCount += 1;
            }
        }
    }

    return ret;
}

bool VKImage::IsFullRange(VkImageSubresourceRange subresourceRange)
{
    return false; // TODO
}

VKImage::BarrierTrack& VKImage::GetBarrierTrack(int layer, int mip)
{
    return subresourceBarrierTrack[layer * imageDescription.mipLevels + mip];
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
