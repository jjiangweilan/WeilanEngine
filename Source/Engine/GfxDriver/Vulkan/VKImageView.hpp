#include "../ImageView.hpp"
#include "VKImage.hpp"
#include <vulkan/vulkan.h>

namespace Engine::Gfx
{
class VKImageView : public ImageView
{
public:
    VKImageView(const CreateInfo& createInfo);
    ~VKImageView() override;
    VkImageView GetHandle()
    {
        return handle;
    }

    Image& GetImage() override
    {
        return image;
    };
    const ImageSubresourceRange& GetSubresourceRange() override
    {
        return subresourceRange;
    }

private:
    VKImage& image;
    VkImageView handle;
    ImageSubresourceRange subresourceRange;
};
} // namespace Engine::Gfx
