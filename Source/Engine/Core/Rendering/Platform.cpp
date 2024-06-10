#include "Platform.hpp"

namespace Rendering
{
Gfx::ImageFormat Platform::GetRelaxedImageFormat(Gfx::ImageFormat desired, Gfx::ImageUsageFlags usages)
{
    auto gfxDriver = GetGfxDriver();

    if (gfxDriver->IsFormatAvaliable(desired, usages))
        return desired;

    switch (desired)
    {
        case Gfx::ImageFormat::D24_UNorm_S8_UInt:
            {
                auto best = GetBetterImageFormat(desired, usages);
                if (best != Gfx::ImageFormat::Invalid)
                    return best;

                if (gfxDriver->IsFormatAvaliable(Gfx::ImageFormat::D16_UNorm_S8_UInt, usages))
                {
                    return Gfx::ImageFormat::D16_UNorm_S8_UInt;
                }
            }
            break;
        default: break;
    }

    return Gfx::ImageFormat::Invalid;
}

Gfx::ImageFormat Platform::GetBetterImageFormat(Gfx::ImageFormat desired, Gfx::ImageUsageFlags usages)
{
    auto gfxDriver = GetGfxDriver();

    switch (desired)
    {
        case Gfx::ImageFormat::D24_UNorm_S8_UInt:
            {
                if (gfxDriver->IsFormatAvaliable(Gfx::ImageFormat::D32_SFLOAT_S8_UInt, usages))
                    return Gfx::ImageFormat::D32_SFLOAT_S8_UInt;
            }
            break;
        default: break;
    }

    return Gfx::ImageFormat::Invalid;
}

Gfx::ImageFormat Platform::GetImageFormat(
    Gfx::ImageFormat desired, Gfx::ImageUsageFlags usages, ImageFormatPicker picker
)
{
    auto gfxDriver = GetGfxDriver();

    if (gfxDriver->IsFormatAvaliable(desired, usages))
        return desired;

    switch (picker)
    {
        case ImageFormatPicker::Best: return GetBetterImageFormat(desired, usages); break;
        case ImageFormatPicker::Relaxed: return GetRelaxedImageFormat(desired, usages); break;
    }

    return Gfx::ImageFormat::Invalid;
}
} // namespace Rendering
