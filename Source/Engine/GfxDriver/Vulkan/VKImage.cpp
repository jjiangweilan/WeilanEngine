#include "VKImage.hpp"
#include "Internal/VKEnumMapper.hpp"
#include "Internal/VKMemAllocator.hpp"
#include "Internal/VKObjectManager.hpp"
#include "Internal/VKUtils.hpp"
#include "VKContext.hpp"
#include "VKDebugUtils.hpp"
#include <vma/vk_mem_alloc.h>

#include <spdlog/spdlog.h>

namespace Engine::Gfx
{
VKImage::VKImage(const ImageDescription& imageDescription, ImageUsageFlags usageFlags)
    : usageFlags(MapImageUsage(usageFlags)), imageDescription(imageDescription)
{
    defaultSubResourceRange = GenerateDefaultSubresourceRange();
    format_vk = MapFormat(imageDescription.format);

    if (imageDescription.data) this->usageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    MakeVkObjects();
    defaultSubResourceRange = GenerateDefaultSubresourceRange();

    SetName("Unnamed");
}

VKImage::VKImage(VKImage&& other)
    : mipLevels(other.mipLevels), arrayLayers(other.arrayLayers), imageType_vk(other.imageType_vk),
      usageFlags(other.usageFlags), image_vk(std::exchange(other.image_vk, VK_NULL_HANDLE)),
      imageView_vk(std::exchange(other.imageView_vk, VK_NULL_HANDLE)),
      defaultSubResourceRange(other.defaultSubResourceRange),
      allocation_vma(std::exchange(other.allocation_vma, VK_NULL_HANDLE)), layout(other.layout),
      stageMask(other.stageMask), accessMask(other.accessMask), imageDescription(other.imageDescription)
{}

VKImage::~VKImage()
{
    if (imageView_vk != VK_NULL_HANDLE) VKContext::Instance()->objManager->DestroyImageView(imageView_vk);
    if (image_vk != VK_NULL_HANDLE && allocation_vma != nullptr)
        VKContext::Instance()->allocator->DestoryImage(image_vk, allocation_vma);
}

void VKImage::FillMemoryBarrierIfNeeded(std::vector<VkImageMemoryBarrier>& barriers,
                                        VkImageLayout newLayout,
                                        VkPipelineStageFlags dstStageMask,
                                        VkAccessFlags dstAccessMask,
                                        const VkImageSubresourceRange* subresourceRange)
{
    if (this->layout != newLayout)
    {
        barriers.emplace_back();
        VkImageMemoryBarrier& barrier = barriers.back();
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.pNext = VK_NULL_HANDLE;
        barrier.srcAccessMask = this->accessMask;
        barrier.dstAccessMask = dstAccessMask;
        barrier.oldLayout = this->layout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image_vk;

        if (subresourceRange) barrier.subresourceRange = *subresourceRange;
        else barrier.subresourceRange = defaultSubResourceRange;

        this->layout = newLayout;
        this->stageMask = dstStageMask;
        this->accessMask = dstAccessMask;
    }
}

void VKImage::TransformLayoutIfNeeded(VkCommandBuffer cmdBuf,
                                      VkImageLayout layout,
                                      VkPipelineStageFlags srcStageMask,
                                      VkPipelineStageFlags dstStageMask,
                                      VkAccessFlags srcAccessMask,
                                      VkAccessFlags dstAccessMask)
{
    TransformLayoutIfNeeded(cmdBuf,
                            layout,
                            srcStageMask,
                            dstStageMask,
                            srcAccessMask,
                            dstAccessMask,
                            defaultSubResourceRange);
}

void VKImage::TransformLayoutIfNeeded(VkCommandBuffer cmdBuf,
                                      VkImageLayout targetLayout,
                                      VkPipelineStageFlags srcStageMask,
                                      VkPipelineStageFlags dstStageMask,
                                      VkAccessFlags srcAccessMask,
                                      VkAccessFlags dstAccessMask,
                                      const VkImageSubresourceRange& subresourceRange)
{
    if (layout != targetLayout)
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.pNext = VK_NULL_HANDLE;
        barrier.srcAccessMask = srcAccessMask;
        barrier.dstAccessMask = dstAccessMask;
        barrier.oldLayout = layout;
        barrier.newLayout = targetLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image_vk;
        barrier.subresourceRange = subresourceRange;
        vkCmdPipelineBarrier(cmdBuf,
                             srcStageMask,
                             dstStageMask,
                             VK_DEPENDENCY_BY_REGION_BIT,
                             0,
                             VK_NULL_HANDLE,
                             0,
                             VK_NULL_HANDLE,
                             1,
                             &barrier);

        layout = targetLayout;
        stageMask = dstStageMask;
        accessMask = dstAccessMask;
    }
}

void VKImage::TransformLayoutIfNeeded(VkCommandBuffer cmdBuf,
                                      VkImageLayout newLayout,
                                      VkPipelineStageFlags dstStageMask,
                                      VkAccessFlags dstAccessMask,
                                      const VkImageSubresourceRange* subresourceRange)
{
    if (this->layout != newLayout)
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.pNext = VK_NULL_HANDLE;
        barrier.srcAccessMask = this->accessMask;
        barrier.dstAccessMask = dstAccessMask;
        barrier.oldLayout = this->layout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image_vk;

        if (subresourceRange) barrier.subresourceRange = *subresourceRange;
        else barrier.subresourceRange = defaultSubResourceRange;
        vkCmdPipelineBarrier(cmdBuf,
                             this->stageMask,
                             dstStageMask,
                             VK_DEPENDENCY_BY_REGION_BIT,
                             0,
                             VK_NULL_HANDLE,
                             0,
                             VK_NULL_HANDLE,
                             1,
                             &barrier);

        this->layout = newLayout;
        this->stageMask = dstStageMask;
        this->accessMask = dstAccessMask;
    }
}

void VKImage::MakeVkObjects()
{
    // create the image
    VkImageCreateInfo imageCreateInfo;
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.pNext = VK_NULL_HANDLE;
    imageCreateInfo.flags = 0;
    imageCreateInfo.imageType = imageType_vk;
    imageCreateInfo.format = format_vk;
    imageCreateInfo.extent = {imageDescription.width, imageDescription.height, 1};
    imageCreateInfo.mipLevels = mipLevels;
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

    CreateImageView();
}

void VKImage::CreateImageView()
{
    VkImageViewCreateInfo createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.pNext = VK_NULL_HANDLE;
    createInfo.flags = 0;
    createInfo.image = image_vk;
    createInfo.viewType = GenerateDefaultImageViewViewType();
    createInfo.format = format_vk;
    createInfo.components = {VK_COMPONENT_SWIZZLE_IDENTITY,
                             VK_COMPONENT_SWIZZLE_IDENTITY,
                             VK_COMPONENT_SWIZZLE_IDENTITY,
                             VK_COMPONENT_SWIZZLE_IDENTITY};
    createInfo.subresourceRange = GenerateDefaultSubresourceRange();

    VKContext::Instance()->objManager->CreateImageView(createInfo, imageView_vk);
}

ImageSubresourceRange VKImage::GetSubresourceRange()
{
    ImageSubresourceRange range;
    range.aspectMask = ImageAspect::None;

    bool hasDepth = VKUtils::FormatHasDepth(format_vk);
    bool hasStencil = VKUtils::FormatHasStencil(format_vk);
    if (hasDepth) range.aspectMask |= ImageAspect::Depth;
    if (hasStencil) range.aspectMask |= ImageAspect::Stencil;
    if (!hasDepth && !hasStencil) range.aspectMask |= ImageAspect::Color;

    range.baseMipLevel = 0;
    range.levelCount = mipLevels;
    range.baseArrayLayer = 0;
    range.layerCount = arrayLayers;

    return range;
}

VkImageSubresourceRange VKImage::GenerateDefaultSubresourceRange()
{
    VkFormat format_vk = MapFormat(imageDescription.format);
    VkImageSubresourceRange range;

    range.aspectMask = 0;
    bool hasDepth = VKUtils::FormatHasDepth(format_vk);
    bool hasStencil = VKUtils::FormatHasStencil(format_vk);
    if (hasDepth) range.aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
    if (hasStencil) range.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    if (!hasDepth && !hasStencil) range.aspectMask |= VK_IMAGE_ASPECT_COLOR_BIT;

    range.baseMipLevel = 0;
    range.levelCount = mipLevels;
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
VkImageViewType VKImage::GenerateDefaultImageViewViewType()
{
    switch (imageType_vk)
    {
        case VK_IMAGE_TYPE_1D: return VK_IMAGE_VIEW_TYPE_1D;
        case VK_IMAGE_TYPE_2D: return VK_IMAGE_VIEW_TYPE_2D;
        case VK_IMAGE_TYPE_3D: return VK_IMAGE_VIEW_TYPE_3D;
        default: break;
    }

    return VK_IMAGE_VIEW_TYPE_2D;
}

VkImageView VKImage::GetDefaultImageView() { return imageView_vk; }
} // namespace Engine::Gfx
