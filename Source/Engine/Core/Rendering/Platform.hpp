#pragma once
#include "GfxDriver/GfxDriver.hpp"

namespace Rendering
{
class Platform
{
public:
    enum class ImageFormatPicker
    {
        Best,   // select at least better that what desired
        Relaxed // select any thing equivalent if desired is not avaliable
    };

    static Gfx::ImageFormat GetImageFormat(
        Gfx::ImageFormat desired, Gfx::ImageUsageFlags usages, ImageFormatPicker picker = ImageFormatPicker::Relaxed
    );

private:
    static Gfx::ImageFormat GetBetterImageFormat(Gfx::ImageFormat desired, Gfx::ImageUsageFlags usages);
    static Gfx::ImageFormat GetRelaxedImageFormat(Gfx::ImageFormat desired, Gfx::ImageUsageFlags usages);
};
} // namespace Rendering
