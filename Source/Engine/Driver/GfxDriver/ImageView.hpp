#pragma once
#include "Engine/Core/Object.hpp"
#include "GfxEnums.hpp"
#include "Image.hpp"

namespace Gfx
{
class ImageView : public Object
{
public:
    ImageView() : Object() {};
    struct CreateInfo
    {
        Image* image = nullptr;
        ImageViewType imageViewType = Gfx::ImageViewType::Image_2D;
        ImageSubresourceRange subresourceRange = {};
    };

    virtual ~ImageView() {};
    virtual Image& GetImage() = 0;
    virtual const ImageSubresourceRange& GetSubresourceRange() = 0;
    virtual ImageViewType GetImageViewType() = 0;
};
} // namespace Gfx
