#include "../ImageView.hpp"
#include "VKImage.hpp"
#include <vulkan/vulkan.h>

namespace Engine::Gfx
{
class VKImageView : public ImageView
{
public:
    VKImageView(const CreateInfo& createInfo);
    VKImageView(const VKImageView& imageView) = delete;
    VKImageView(VKImageView&& other);
    ~VKImageView() override;

    // causion!!! this is used to work around the design of VKSwapchainImage and VKSwapchainImageProxy
    // don't use this API for other purpose
    void ChangeOwner(VKImage* image)
    {
        this->image = image;
    }

public:
    VkImageView GetHandle()
    {
        return handle;
    }

    Image* GetImage() override
    {
        return image;
    };
    const ImageSubresourceRange& GetSubresourceRange() override
    {
        return subresourceRange;
    }

    VkImageSubresourceRange GetVkSubresourceRange();

private:
    VKImage* image;
    VkImageView handle = VK_NULL_HANDLE;
    ImageSubresourceRange subresourceRange;
};
} // namespace Engine::Gfx
