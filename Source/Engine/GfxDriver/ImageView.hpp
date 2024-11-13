#pragma once
#include "GfxEnums.hpp"
#include "Image.hpp"
#include "Core/Object.hpp"

namespace Gfx
{
class ImageView : public Object
{
public:
    struct CreateInfo
    {
        Image& image;
        ImageViewType imageViewType;
        ImageSubresourceRange subresourceRange;
    };

    virtual void SetName(std::string_view name) = 0;
    virtual ~ImageView(){};
    virtual Image& GetImage() = 0;
    virtual const ImageSubresourceRange& GetSubresourceRange() = 0;
    virtual ImageViewType GetImageViewType() = 0;
};
} // namespace Gfx
