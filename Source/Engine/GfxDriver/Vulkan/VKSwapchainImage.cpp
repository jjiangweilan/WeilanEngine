#include "VKSwapchainImage.hpp"
#include "Internal/VKEnumMapper.hpp"
#include "VKImageView.hpp"
#include "fmt/format.h"

namespace Gfx
{
VKSwapChainImage::~VKSwapChainImage()
{
    image_vk = VK_NULL_HANDLE; // avoid swaph chain image being deleted by VKImage
}

void VKSwapChainImage::Recreate(
    std::span<VkImage> swapchainImages, VkFormat format, uint32_t width, uint32_t height, VkImageUsageFlags usageFlags
)
{
    this->swapchainImages.clear();

    format_vk = format;
    imageDescription.format = MapVKFormat(format);
    imageDescription.width = width;
    imageDescription.height = height;
    imageDescription.mipLevels = 1;
    layout = VK_IMAGE_LAYOUT_UNDEFINED;

    for (auto image : swapchainImages)
    {
        VKImage* ptr = new VKImage(
            image,
            {
                .width = width,
                .height = height,
                .format = MapVKFormat(format),
                .multiSampling = Gfx::MultiSampling::Sample_Count_1,
                .mipLevels = 1,
                .isCubemap = false,
            },
            usageFlags
        );
        // static_cast<VKImageView&>(ptr->GetDefaultImageView()).ChangeOwner(this);
        this->swapchainImages.emplace_back(ptr);
    }

    if (imageView == nullptr)
    {
        imageView = std::make_unique<VKSwapchainImageView>();
    }
    ((VKSwapchainImageView*)imageView.get())->SetSwapchainImage(*this);
}

// VkImageView VKSwapChainImage::GetDefaultVkImageView()
// {
//     return static_cast<VKImageView*>(&swapchainImages[activeIndex]->GetDefaultImageView())->GetHandle();
// }

void VKSwapChainImage::SetName(std::string_view name)
{
    int i = 0;
    for (auto& image : swapchainImages)
    {
        image->SetName(fmt::format("{} {}", name, i));
        i += 1;
    }
}

} // namespace Gfx
