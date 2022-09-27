#pragma once

#include "../Image.hpp"
#include "GfxDriver/GfxEnums.hpp"
#include "Code/Ptr.hpp"
#include <vma/vk_mem_alloc.h>
#include <cinttypes>
#include <vulkan/vulkan.h>
namespace Engine::Gfx
{
    struct VKContext;
    class VKImage : public Image
    {
        public:
            VKImage(RefPtr<VKContext> context,
                    const ImageDescription& imageDescription,
                    ImageUsageFlags usageFlags
                    );
            VKImage(const VKImage& other) = delete;
            VKImage(VKImage&& other);
            
            ~VKImage() override;

            // defualt image view created according to the creation parameters
            VkImageView GetDefaultImageView();

            VkImageSubresourceRange GetDefaultSubresourceRange() {return defaultSubResourceRange;}
            VkImage GetImage() {return image_vk;}
            VkImageLayout GetLayout() {return layout;}
            const ImageDescription& GetDescription() override { return imageDescription; }

            void TransformLayoutIfNeeded(VkCommandBuffer cmdBuf, VkImageLayout layout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dskStageMask, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, const VkImageSubresourceRange& subresourceRange);
            void TransformLayoutIfNeeded(VkCommandBuffer cmdBuf, VkImageLayout layout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dskStageMask, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask);

        protected:
            VKImage(){};
            uint32_t mipLevels = 1;
            uint32_t arrayLayers = 1;
            uint32_t queueFamilyIndex;
            VkImageType imageType_vk = VK_IMAGE_TYPE_2D;
            VkImageUsageFlags usageFlags;
            VkImage image_vk = VK_NULL_HANDLE;
            VkImageView imageView_vk = VK_NULL_HANDLE;
            VkDevice device_vk = VK_NULL_HANDLE;
            VkFormat format_vk = VK_FORMAT_UNDEFINED;
            VkImageSubresourceRange defaultSubResourceRange;
            RefPtr<VKContext> context = nullptr;

            VmaAllocationInfo allocationInfo_vma;
            VmaAllocation allocation_vma = nullptr;

            VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
            ImageDescription imageDescription;
            VkImageViewType GenerateDefaultImageViewViewType();
            VkImageSubresourceRange GenerateDefaultSubresourceRange();
            void MakeVkObjects();
            void CreateImageView();
    };
}
