#pragma once
#include "Image.hpp"

namespace Engine::Gfx
{
enum class ImageViewType
{
    Image_1D,
    Image_2D,
    Image_3D,
    Cubemap,
    Image_1D_Array,
    Image_2D_Array,
    Cube_Array,
};
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
