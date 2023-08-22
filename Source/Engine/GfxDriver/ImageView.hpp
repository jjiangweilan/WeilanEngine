#pragma once
#include "GfxEnums.hpp"
#include "Image.hpp"

namespace Engine::Gfx
{
class ImageView
{
public:
    struct CreateInfo
    {
        Image& image;
        ImageViewType imageViewType;
        ImageSubresourceRange subresourceRange;
    };
    virtual ~ImageView(){};
    virtual Image& GetImage() = 0;
    virtual const ImageSubresourceRange& GetSubresourceRange() = 0;
};
} // namespace Engine::Gfx
