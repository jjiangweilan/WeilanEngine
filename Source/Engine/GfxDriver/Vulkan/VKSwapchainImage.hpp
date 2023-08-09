#pragma once
#include "VKImage.hpp"
#include "VKImageView.hpp"

namespace Engine::Gfx
{
class VKSwapChainImage : public VKImage
{
public:
    VKSwapChainImage() = default;
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

    void SetActiveSwapChainImage(uint32_t index)
    {
        this->activeIndex = index;
    }

    uint32_t GetActiveIndex()
    {
        return activeIndex;
    }

    VkImageView GetDefaultVkImageView() override;

    VkImage GetImage() override
    {
        return swapchainImages[activeIndex]->GetImage();
    }

    VkImageSubresourceRange GetDefaultSubresourceRange() override
    {
        return static_cast<VKImageView&>(swapchainImages[activeIndex]->GetDefaultImageView()).GetVkSubresourceRange();
    }

    ImageView& GetDefaultImageView() override
    {
        return swapchainImages[activeIndex]->GetDefaultImageView();
    }

    void SetName(std::string_view name) override;

    VKImage* GetImage(uint32_t index)
    {
        return swapchainImages[activeIndex].get();
    }

private:
    std::vector<std::unique_ptr<VKImage>> swapchainImages;
    int activeIndex;
};
} // namespace Engine::Gfx
