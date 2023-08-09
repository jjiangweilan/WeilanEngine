#pragma once

#include "../Image.hpp"
#include "GfxDriver/GfxEnums.hpp"
#include "Internal/VKDevice.hpp"
#include "Internal/VKMemAllocator.hpp"
#include "Internal/VKObjectManager.hpp"
#include "Libs/Ptr.hpp"

#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <cinttypes>
#include <string>
#include <vector>
namespace Engine::Gfx
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
    virtual const ImageDescription& GetDescription() override
    {
        return imageDescription;
    }
    virtual ImageSubresourceRange GetSubresourceRange() override;

    virtual VkImageSubresourceRange GetDefaultSubresourceRange();
    virtual void SetName(std::string_view name) override;

    // used in command buffer
    void NotifyLayoutChange(VkImageLayout newLayout)
    {
        this->layout = newLayout;
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

    ImageViewType GenerateDefaultImageViewViewType();
    ImageSubresourceRange GenerateDefaultSubresourceRange();
    void MakeVkObjects();
    void CreateImageView();
};

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
} // namespace Engine::Gfx
