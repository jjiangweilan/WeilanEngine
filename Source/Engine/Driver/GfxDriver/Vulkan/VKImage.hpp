#pragma once

#include "../Image.hpp"
#include "Engine/Core/Ptr.hpp"
#include "Engine/Driver/GfxDriver/GfxEnums.hpp"
#include "Internal/VKDevice.hpp"
#include "Internal/VKMemAllocator.hpp"
#include "Internal/VKObjectManager.hpp"
#include "VKUtils.hpp"

#include "Engine/Driver/GfxDriver/Vulkan/VKCommon.hpp"
#include <vulkan/vulkan_hash.hpp>

#include "Engine/Library/DynamicArray.hpp"
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
    std::unordered_map<vk::ImageViewCreateInfo, std::unique_ptr<VKImageView>> imageViews;
    bool isSwapchainProxy = false;

    std::vector<VkImageLayout> layoutTrack; // Deprecated: we should be moving to subresourceBarrierTrack
    std::vector<BarrierTrack> subresourceBarrierTrack;

public:
    VKImage();
    VKImage(const ImageDescription& imageDescription, ImageUsageFlags usageFlags);
    VKImage(VkImage image, const ImageDescription& imageDescription, ImageUsageFlags usageFlags);
    VKImage(const VKImage& other) = delete;
    VKImage(VKImage&& other);
    ~VKImage() override;
    ImageView& GetDefaultImageView() override;
    ImageView& GetImageView(const ImageViewOption& option) override;
    virtual VkImageView GetDefaultVkImageView();
    virtual VkImage GetImage();

    void SetData(std::span<uint8_t> binaryData, uint32_t mip, uint32_t layer) override;
    void SetData(std::span<uint8_t> binaryData, uint32_t mip, uint32_t layer, VkImageLayout finalLayout);

    virtual ImageView& GetDefaultImageViewForShaderResource() override;

    virtual const ImageDescription& GetDescription() override;
    virtual ImageSubresourceRange GetSubresourceRange() override;

    virtual VkImageSubresourceRange GetDefaultSubresourceRange();
    virtual void SetName(std::string_view name) override;
    virtual const std::string& GetName() const override;

    bool IsSwapchainProxy();

    void SetLayout(VkImageSubresourceRange subresourceRange, VkImageLayout layout,
        VkPipelineStageFlags2 srcStageMask,
        VkAccessFlags2 srcAccessMask,
        VkPipelineStageFlags2 dstStageMask,
        VkAccessFlags2 dstAccessMask
        );

    bool IsLayout(VkImageSubresourceRange subresourceRange, VkImageLayout layout);

    bool QueryLayout(VkImageSubresourceRange subresourceRange, VkImageLayout& layout);

    std::vector<VkImageMemoryBarrier2> MakeBarrierIfNeeded(VkPipelineStageFlags2 stageFlags, VkAccessFlags2 accessFlags, VkImageLayout expectedImageLayout, VkImageSubresourceRange subresourceRange);

protected:

    void InitBarrierTrack();
    ImageViewType GenerateDefaultImageViewViewType();
    ImageSubresourceRange GenerateDefaultSubresourceRange();
    void MakeVkObjects();
    void CreateImageView();

private:
    bool IsFullRange(VkImageSubresourceRange subresourceRange);
    BarrierTrack& GetBarrierTrack(int layer, int mip);
    void SafeMipRange(uint32_t& baseMipLevel, uint32_t& levelCount);

    friend class VKDriver;
};

VkImageSubresourceRange MapVkImageSubresourceRange(const ImageSubresourceRange& range);
} // namespace Gfx
