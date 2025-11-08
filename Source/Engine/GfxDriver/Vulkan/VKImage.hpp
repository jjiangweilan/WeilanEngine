#pragma once

#include "../Image.hpp"
#include "Core/Ptr.hpp"
#include "GfxDriver/GfxEnums.hpp"
#include "Internal/VKDevice.hpp"
#include "Internal/VKMemAllocator.hpp"
#include "Internal/VKObjectManager.hpp"
#include "VKUtils.hpp"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_hash.hpp>

#include "Libs/DynamicArray.hpp"
#include <cinttypes>
#include <string>
namespace Gfx
{
class VKImageView;
class VKImage : public Image
{
    DECLARE_OBJECT();

protected:
    struct BarrierTrack
    {
        VkPipelineStageFlags2 srcStageMask;
        VkAccessFlags2 srcAccessMask;
        VkPipelineStageFlags2 dstStageMask;
        VkAccessFlags2 dstAccessMask;
        VkImageLayout oldLayout;
        VkImageLayout newLayout;
    };

    uint32_t arrayLayers = 1;
    VkImageType imageType_vk = VK_IMAGE_TYPE_2D;
    VkImageUsageFlags usageFlags;
    VkImage image_vk = VK_NULL_HANDLE;
    VkFormat format_vk = VK_FORMAT_UNDEFINED;
    VkImageSubresourceRange defaultSubResourceRange;

    VmaAllocationInfo allocationInfo_vma;
    VmaAllocation allocation_vma = nullptr;

    VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkAccessFlags accessMask = VK_ACCESS_MEMORY_READ_BIT;
    ImageDescription imageDescription;
    std::unique_ptr<VKImageView> imageView;
    std::unique_ptr<VKImageView> imageViewForShaderResource = nullptr;
    std::string name;
    std::vector<VkImageLayout> layoutTrack;
    std::unordered_map<vk::ImageViewCreateInfo, std::unique_ptr<VKImageView>> imageViews;
    bool isSwapchainProxy = false;

    std::vector<BarrierTrack> subresourceBarrierTrack;

public:
    VKImage(const ImageDescription& imageDescription, ImageUsageFlags usageFlags);
    VKImage(VkImage image, const ImageDescription& imageDescription, ImageUsageFlags usageFlags);
    VKImage(const VKImage& other) = delete;
    VKImage(VKImage&& other);
    ~VKImage() override;
    ImageView& GetDefaultImageView() override;
    ImageView& GetImageView(const ImageViewOption& option) override;
    virtual VkImageView GetDefaultVkImageView();
    virtual VkImage GetImage()
    {
        return image_vk;
    }

    void SetData(std::span<uint8_t> binaryData, uint32_t mip, uint32_t layer) override;
    void SetData(std::span<uint8_t> binaryData, uint32_t mip, uint32_t layer, VkImageLayout finalLayout);

    virtual ImageView& GetDefaultImageViewForShaderResource() override;

    virtual const ImageDescription& GetDescription() override
    {
        return imageDescription;
    }
    virtual ImageSubresourceRange GetSubresourceRange() override;

    virtual VkImageSubresourceRange GetDefaultSubresourceRange();
    virtual void SetName(std::string_view name) override;
    virtual const std::string& GetName() const override
    {
        return name;
    };

    bool IsSwapchainProxy()
    {
        return isSwapchainProxy;
    }

    void SetLayout(VkImageSubresourceRange subresourceRange, VkImageLayout layout)
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

    bool IsLayout(VkImageSubresourceRange subresourceRange, VkImageLayout layout)
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

    bool QueryLayout(VkImageSubresourceRange subresourceRange, VkImageLayout& layout)
    {
        layout =
            layoutTrack[subresourceRange.baseArrayLayer * imageDescription.mipLevels + subresourceRange.baseMipLevel];

        return IsLayout(subresourceRange, layout);
    }

    std::vector<VkImageMemoryBarrier2> MakeBarrierIfNeeded(VkPipelineStageFlags2 stageFlags, VkAccessFlags2 accessFlags, VkImageLayout expectedImageLayout, VkImageSubresourceRange subresourceRange)
    {

        std::vector<VkImageMemoryBarrier2> ret{};

        if (IsFullRange(subresourceRange))
        {
            // TODO: do full range track until separate track is needed
            return {};
        }

        for (int layerIdx = subresourceRange.baseArrayLayer; layerIdx < subresourceRange.layerCount; ++layerIdx)
        {
            for (int mipIdx = subresourceRange.baseMipLevel; mipIdx < subresourceRange.levelCount; ++mipIdx)
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

                    subresourceBarrier.srcStageMask = barrier.srcStageMask;
                    subresourceBarrier.srcAccessMask = barrier.srcAccessMask;
                    subresourceBarrier.dstStageMask = barrier.dstStageMask;
                    subresourceBarrier.dstAccessMask = barrier.dstAccessMask;
                    subresourceBarrier.oldLayout = barrier.oldLayout;
                    subresourceBarrier.newLayout = barrier.newLayout;

                    ret.push_back(barrier);
                }
            }
        }

        return ret;
    }

protected:
    VKImage();

    void InitBarrierTrack();
    ImageViewType GenerateDefaultImageViewViewType();
    ImageSubresourceRange GenerateDefaultSubresourceRange();
    void MakeVkObjects();
    void CreateImageView();

private:
    bool IsFullRange(VkImageSubresourceRange subresourceRange)
    {
        return false; // TODO
    }

    BarrierTrack& GetBarrierTrack(int layer, int mip)
    {
        return subresourceBarrierTrack[layer * imageDescription.mipLevels + mip];
    }

    friend class VKDriver;
};

VkImageSubresourceRange MapVkImageSubresourceRange(const ImageSubresourceRange& range);
} // namespace Gfx
