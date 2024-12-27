#include "Platform.hpp"

namespace Rendering
{
Gfx::GfxFormat Platform::GetRelaxedGfxFormat(Gfx::GfxFormat desired, Gfx::ImageUsageFlags usages)
{
    auto gfxDriver = GetGfxDriver();

    if (gfxDriver->IsFormatAvaliable(desired, usages))
        return desired;

    switch (desired)
    {
        case Gfx::GfxFormat::D24_UNorm_S8_UInt:
            {
                auto best = GetBetterGfxFormat(desired, usages);
                if (best != Gfx::GfxFormat::Invalid)
                    return best;

                if (gfxDriver->IsFormatAvaliable(Gfx::GfxFormat::D16_UNorm_S8_UInt, usages))
                {
                    return Gfx::GfxFormat::D16_UNorm_S8_UInt;
                }
            }
            break;
        default: break;
    }

    return Gfx::GfxFormat::Invalid;
}

Gfx::GfxFormat Platform::GetBetterGfxFormat(Gfx::GfxFormat desired, Gfx::ImageUsageFlags usages)
{
    auto gfxDriver = GetGfxDriver();

    switch (desired)
    {
        case Gfx::GfxFormat::D24_UNorm_S8_UInt:
            {
                if (gfxDriver->IsFormatAvaliable(Gfx::GfxFormat::D32_SFLOAT_S8_UInt, usages))
                    return Gfx::GfxFormat::D32_SFLOAT_S8_UInt;
            }
            break;
        default: break;
    }

    return Gfx::GfxFormat::Invalid;
}

Gfx::GfxFormat Platform::GetGfxFormat(
    Gfx::GfxFormat desired, Gfx::ImageUsageFlags usages, GfxFormatPicker picker
)
{
    auto gfxDriver = GetGfxDriver();

    if (gfxDriver->IsFormatAvaliable(desired, usages))
        return desired;

    switch (picker)
    {
        case GfxFormatPicker::Best: return GetBetterGfxFormat(desired, usages); break;
        case GfxFormatPicker::Relaxed: return GetRelaxedGfxFormat(desired, usages); break;
    }

    return Gfx::GfxFormat::Invalid;
}
} // namespace Rendering
