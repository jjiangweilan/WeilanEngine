#pragma once
#include "../ImageView.hpp"
#include "VKDebugUtils.hpp"
#include "VKImage.hpp"
#include <vulkan/vulkan.h>

namespace Gfx
{
VkImageViewCreateInfo MapImageViewCreateInfo(VKImage* image, const ImageView::CreateInfo& createInfo);
class VKImageView : public ImageView
{
    DECLARE_OBJECT();
public:
    VKImageView() = default;
    VKImageView(const CreateInfo& createInfo);
    VKImageView(const VKImageView& imageView) = delete;
    VKImageView(VKImageView&& other);
    ~VKImageView() override;

    void SetName(std::string_view name) override
    {
        VKDebugUtils::SetDebugName(VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)handle, name.data());
    }

    // causion!!! this is used to work around the design of VKSwapchainImage and VKSwapchainImageProxy
    // don't use this API for other purpose
    void ChangeOwner(VKImage* image)
    {
        this->image = image;
    }

public:
    virtual VkImageView GetHandle()
    {
        return handle;
    }

    Image& GetImage() override
    {
        return *image;
    };

    const ImageSubresourceRange& GetSubresourceRange() override
    {
        return subresourceRange;
    }

    virtual VkImageSubresourceRange GetVkSubresourceRange();

    ImageViewType GetImageViewType() override
    {
        return imageViewType;
    }

    size_t GetHash() const
    {
        return hash;
    }

protected:

    size_t hash;
    VKImage* image;
    VkImageView handle = VK_NULL_HANDLE;
    ImageSubresourceRange subresourceRange;
    ImageViewType imageViewType;
};
} // namespace Gfx
