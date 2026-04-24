#pragma once
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"

namespace Rendering
{
class Platform
{
public:
    enum class GfxFormatPicker
    {
        Best,   // select at least better that what desired
        Relaxed // select any thing equivalent if desired is not avaliable
    };

    static Gfx::GfxFormat GetGfxFormat(
        Gfx::GfxFormat desired, Gfx::ImageUsageFlags usages, GfxFormatPicker picker = GfxFormatPicker::Relaxed
    );

private:
    static Gfx::GfxFormat GetBetterGfxFormat(Gfx::GfxFormat desired, Gfx::ImageUsageFlags usages);
    static Gfx::GfxFormat GetRelaxedGfxFormat(Gfx::GfxFormat desired, Gfx::ImageUsageFlags usages);
};
} // namespace Rendering
