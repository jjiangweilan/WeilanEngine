#pragma once
#include "../SwapchainImage.hpp"
#include "VKImage.hpp"
#include "VKImageView.hpp"

namespace Gfx
{

class VKSwapchainImage2 : public SwapchainImage
{
public:
    void SetActiveImage(VKImage* image){};
    VKImage* GetActiveImage() override
    {
        return activeImage;
    };

private:
    VKImage* activeImage = nullptr;
};

class VKSwapChainImage : public VKImage
{
public:
    VKSwapChainImage()
    {
        activeIndex = 0;
        imageDescription = {};
        isSwapchainProxy = true;
    }
    VKSwapChainImage(VKSwapChainImage&& other) : VKImage(std::move(other)){};

    ~VKSwapChainImage() override;

public:
    void Recreate(
        std::span<VkImage> swapchainImages,
        VkFormat format,
        uint32_t width,
        uint32_t height,
        VkImageUsageFlags usageFlags
    );

    void NotifyLayoutChange(VkImageLayout newLayout) override
    {
        swapchainImages[activeIndex]->NotifyLayoutChange(newLayout);
    }

    const ImageDescription& GetDescription() override
    {
        return swapchainImages[activeIndex]->GetDescription();
    }

    VkImageLayout GetLayout() override
    {
        return swapchainImages[activeIndex]->GetLayout();
    }

    void SetActiveSwapChainImage(uint32_t index)
    {
        this->activeIndex = index;
    }

    uint32_t GetActiveIndex()
    {
        return activeIndex;
    }

    // VkImageView GetDefaultVkImageView() override;

    VkImage GetImage() override
    {
        return swapchainImages[activeIndex]->GetImage();
    }

    VkImageSubresourceRange GetDefaultSubresourceRange() override
    {
        return static_cast<VKImageView&>(swapchainImages[activeIndex]->GetDefaultImageView()).GetVkSubresourceRange();
    }

    // ImageView& GetDefaultImageView() override
    // {
    //     return imageView swapchainImages[activeIndex]->GetDefaultImageView();
    // }

    void SetName(std::string_view name) override;

    VKImage* GetImage(uint32_t index)
    {
        return swapchainImages[index].get();
    }

private:
    std::vector<std::unique_ptr<VKImage>> swapchainImages;
    int activeIndex;

    friend class VKSwapchainImageView;
};

class VKSwapchainImageView : public VKImageView
{
public:
    VKSwapchainImageView()
    {
        imageViewType = ImageViewType::Image_2D;
    }

    void SetSwapchainImage(VKSwapChainImage& swapchainImage)
    {
        this->swapchainImage = &swapchainImage;
        image = &swapchainImage;
    }

    VkImageView GetHandle() override
    {
        return swapchainImage->swapchainImages[swapchainImage->activeIndex]->GetDefaultVkImageView();
    }

    const ImageSubresourceRange& GetSubresourceRange() override
    {
        return swapchainImage->swapchainImages[swapchainImage->activeIndex]->GetDefaultImageView().GetSubresourceRange(
        );
    }

    VkImageSubresourceRange GetVkSubresourceRange() override
    {
        return static_cast<VKImageView&>(
                   swapchainImage->swapchainImages[swapchainImage->activeIndex]->GetDefaultImageView()
        )
            .GetVkSubresourceRange();
    }

private:
    VKSwapChainImage* swapchainImage;
};
} // namespace Gfx
