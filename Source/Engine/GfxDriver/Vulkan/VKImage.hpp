#pragma once

#include "../Image.hpp"
#include "GfxDriver/GfxEnums.hpp"
#include "Internal/VKDevice.hpp"
#include "Internal/VKMemAllocator.hpp"
#include "Internal/VKObjectManager.hpp"
#include "Libs/Ptr.hpp"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <cinttypes>
#include <string>
#include <vector>
namespace Gfx
{
class VKImageView;
class VKImage : public Image
{
public:
    VKImage(const ImageDescription& imageDescription, ImageUsageFlags usageFlags);
    VKImage(VkImage image, const ImageDescription& imageDescription, ImageUsageFlags usageFlags);
    VKImage(const VKImage& other) = delete;
    VKImage(VKImage&& other);
    ~VKImage() override;
    ImageView& GetDefaultImageView() override;
    virtual VkImageView GetDefaultVkImageView();
    virtual VkImage GetImage()
    {
        return image_vk;
    }
    virtual VkImageLayout GetLayout()
    {
        return layout;
    }

    void SetData(std::span<uint8_t> binaryData, uint32_t mip, uint32_t layer) override;
    void SetData(std::span<uint8_t> binaryData, uint32_t mip, uint32_t layer, VkImageLayout finalLayout);

    virtual ImageLayout GetImageLayout() override;
    virtual const ImageDescription& GetDescription() override
    {
        return imageDescription;
    }
    virtual ImageSubresourceRange GetSubresourceRange() override;

    virtual VkImageSubresourceRange GetDefaultSubresourceRange();
    virtual void SetName(std::string_view name) override;
    virtual const std::string& GetName() override
    {
        return name;
    };

    // used in command buffer
    virtual void NotifyLayoutChange(VkImageLayout newLayout)
    {
        this->layout = newLayout;
    }

    bool IsSwapchainProxy()
    {
        return isSwapchainProxy;
    }

    void SetLayout(VkImageSubresourceRange subresourceRange, VkImageLayout layout)
    {
        int baseIndex = subresourceRange.baseArrayLayer * arrayLayers;
        int count = subresourceRange.levelCount * subresourceRange.layerCount;

        for (int i = baseIndex; i < layoutTrack.size() && i < count; ++i)
        {
            layoutTrack[i] = layout;
        }
    }

    bool QueryLayout(VkImageSubresourceRange subresourceRange, VkImageLayout& layout)
    {
        layout = layoutTrack[subresourceRange.baseArrayLayer * arrayLayers + subresourceRange.baseMipLevel];
        for (int mip = subresourceRange.baseMipLevel; mip < subresourceRange.levelCount; mip++)
        {
            for (int level = subresourceRange.baseArrayLayer; level < subresourceRange.layerCount; ++level)
            {
                if (layoutTrack[level * arrayLayers + mip] != layout)
                    return false;
            }
        }
        return true;
    }

protected:
    VKImage();
    uint32_t arrayLayers = 1;
    VkImageType imageType_vk = VK_IMAGE_TYPE_2D;
    VkImageUsageFlags usageFlags;
    VkImage image_vk = VK_NULL_HANDLE;
    VkFormat format_vk = VK_FORMAT_UNDEFINED;
    VkImageSubresourceRange defaultSubResourceRange;

    VmaAllocationInfo allocationInfo_vma;
    VmaAllocation allocation_vma = nullptr;

    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkAccessFlags accessMask = VK_ACCESS_MEMORY_READ_BIT;
    ImageDescription imageDescription;
    std::unique_ptr<VKImageView> imageView;
    std::string name;
    std::vector<VkImageLayout> layoutTrack;
    bool isSwapchainProxy = false;

    ImageViewType GenerateDefaultImageViewViewType();
    ImageSubresourceRange GenerateDefaultSubresourceRange();
    void MakeVkObjects();
    void CreateImageView();

    friend class VKDriver;
};

VkImageSubresourceRange MapVkImageSubresourceRange(const ImageSubresourceRange& range);

// class VKSwapChainImageProxy : public VKImage
// {
// public:
//     VKSwapChainImageProxy();
//     ~VKSwapChainImageProxy() override;
//
//     void SetActiveSwapChainImage(RefPtr<VKImage> activeImage, uint32_t index);
//
//     void UpdateImageDescription(const ImageDescription& desc)
//     {
//         this->imageDescription = desc;
//     }
//     uint32_t GetActiveIndex()
//     {
//         return activeIndex;
//     }
//     virtual VkImageView GetDefaultVkImageView() override
//     {
//         return activeImage->GetDefaultVkImageView();
//     }
//     virtual VkImage GetImage() override
//     {
//         return activeImage->GetImage();
//     }
//     virtual VkImageLayout GetLayout() override
//     {
//         return activeImage->GetLayout();
//     }
//     virtual VkImageSubresourceRange GetDefaultSubresourceRange() override
//     {
//         return activeImage->GetDefaultSubresourceRange();
//     }
//     virtual ImageView& GetDefaultImageView() override
//     {
//         return activeImage->GetDefaultImageView();
//     }
//
// private:
//     uint32_t activeIndex;
//     RefPtr<VKImage> activeImage;
// };
} // namespace Gfx
