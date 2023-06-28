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
class VKImage : public Image
{
public:
    VKImage(const ImageDescription& imageDescription, ImageUsageFlags usageFlags);
    VKImage(const VKImage& other) = delete;
    VKImage(VKImage&& other);
    ~VKImage() override;

    virtual VkImageView GetDefaultImageView();
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

    virtual VkImageSubresourceRange GetDefaultSubresourceRange()
    {
        return defaultSubResourceRange;
    }
    virtual void SetName(std::string_view name) override;

    // TODO: obsolate
    virtual void TransformLayoutIfNeeded(
        VkCommandBuffer cmdBuf,
        VkImageLayout layout,
        VkPipelineStageFlags srcStageMask,
        VkPipelineStageFlags dstStageMask,
        VkAccessFlags srcAccessMask,
        VkAccessFlags dstAccessMask,
        const VkImageSubresourceRange& subresourceRange
    );
    // TODO: obsolate
    virtual void TransformLayoutIfNeeded(
        VkCommandBuffer cmdBuf,
        VkImageLayout layout,
        VkPipelineStageFlags srcStageMask,
        VkPipelineStageFlags dstStageMask,
        VkAccessFlags srcAccessMask,
        VkAccessFlags dstAccessMask
    );

    virtual void TransformLayoutIfNeeded(
        VkCommandBuffer cmdBuf,
        VkImageLayout layout,
        VkPipelineStageFlags dstStageMask,
        VkAccessFlags dstAccessMask,
        const VkImageSubresourceRange* subresourceRange = nullptr
    );
    virtual void FillMemoryBarrierIfNeeded(
        std::vector<VkImageMemoryBarrier>& barriers,
        VkImageLayout newLayout,
        VkPipelineStageFlags dstStageMask,
        VkAccessFlags dstAccessMask,
        const VkImageSubresourceRange* subresourceRange = nullptr
    );

    void ChangeLayout(VkImageLayout l, VkPipelineStageFlags s, VkAccessFlags a)
    {
        layout = l;
        stageMask = s;
        accessMask = a;
    }

protected:
    VKImage() = default;
    uint32_t arrayLayers = 1;
    VkImageType imageType_vk = VK_IMAGE_TYPE_2D;
    VkImageUsageFlags usageFlags;
    VkImage image_vk = VK_NULL_HANDLE;
    VkImageView imageView_vk = VK_NULL_HANDLE;
    VkFormat format_vk = VK_FORMAT_UNDEFINED;
    VkImageSubresourceRange defaultSubResourceRange;

    VmaAllocationInfo allocationInfo_vma;
    VmaAllocation allocation_vma = nullptr;

    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkAccessFlags accessMask = VK_ACCESS_MEMORY_READ_BIT;
    ImageDescription imageDescription;
    std::string name;

    VkImageViewType GenerateDefaultImageViewViewType();
    VkImageSubresourceRange GenerateDefaultSubresourceRange();
    void MakeVkObjects();
    void CreateImageView();
};

class VKSwapChainImageProxy : public VKImage
{
public:
    VKSwapChainImageProxy() = default;
    ~VKSwapChainImageProxy() = default;

    void SetActiveSwapChainImage(RefPtr<VKImage> activeImage, uint32_t index)
    {
        this->activeImage = activeImage;
        this->activeIndex = index;

        imageDescription = activeImage->GetDescription();
    };

    void UpdateImageDescription(const ImageDescription& desc)
    {
        this->imageDescription = desc;
    }
    uint32_t GetActiveIndex()
    {
        return activeIndex;
    }
    virtual VkImageView GetDefaultImageView() override
    {
        return activeImage->GetDefaultImageView();
    }
    virtual VkImage GetImage() override
    {
        return activeImage->GetImage();
    }
    virtual VkImageLayout GetLayout() override
    {
        return activeImage->GetLayout();
    }
    virtual void TransformLayoutIfNeeded(
        VkCommandBuffer cmdBuf,
        VkImageLayout layout,
        VkPipelineStageFlags srcStageMask,
        VkPipelineStageFlags dskStageMask,
        VkAccessFlags srcAccessMask,
        VkAccessFlags dstAccessMask,
        const VkImageSubresourceRange& subresourceRange
    ) override
    {
        activeImage->TransformLayoutIfNeeded(
            cmdBuf,
            layout,
            srcStageMask,
            dskStageMask,
            srcAccessMask,
            dstAccessMask,
            subresourceRange
        );
    }
    virtual void TransformLayoutIfNeeded(
        VkCommandBuffer cmdBuf,
        VkImageLayout layout,
        VkPipelineStageFlags srcStageMask,
        VkPipelineStageFlags dskStageMask,
        VkAccessFlags srcAccessMask,
        VkAccessFlags dstAccessMask
    ) override
    {
        activeImage->TransformLayoutIfNeeded(cmdBuf, layout, srcStageMask, dskStageMask, srcAccessMask, dstAccessMask);
    }

    virtual void TransformLayoutIfNeeded(
        VkCommandBuffer cmdBuf,
        VkImageLayout layout,
        VkPipelineStageFlags dstStageMask,
        VkAccessFlags dstAccessMask,
        const VkImageSubresourceRange* subresourceRange = nullptr
    ) override
    {
        activeImage->TransformLayoutIfNeeded(cmdBuf, layout, dstStageMask, dstAccessMask, subresourceRange);
    }

    virtual const ImageDescription& GetDescription() override
    {
        return activeImage->GetDescription();
    }
    virtual VkImageSubresourceRange GetDefaultSubresourceRange() override
    {
        return activeImage->GetDefaultSubresourceRange();
    }

private:
    uint32_t activeIndex;
    RefPtr<VKImage> activeImage;
};
} // namespace Engine::Gfx
