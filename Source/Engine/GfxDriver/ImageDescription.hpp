#pragma once
#include "GfxEnums.hpp"
#include "Libs/Math.hpp"

namespace Gfx
{

struct ImageDescription
{
    ImageDescription() = default;
    ImageDescription(
        uint32_t width,
        uint32_t height,
        uint32_t depth,
        Gfx::GfxFormat format,
        Gfx::MultiSampling multiSampling,
        uint32_t mipLevels,
        bool isCubemap
    )
        : width(width), height(height), depth(depth), format(format), multiSampling(multiSampling),
          mipLevels(mipLevels), isCubemap(isCubemap)
    {
        if (isCubemap)
        {
            layers = 6;
        }
    }

    ImageDescription(uint32_t width, uint32_t height, Gfx::GfxFormat format)
        : width(width), height(height), depth(1.0f), format(format), multiSampling(MultiSampling::Sample_Count_1),
          mipLevels(1), isCubemap(false)
    {}

    ImageDescription(uint32_t width, uint32_t height, uint32_t depth, Gfx::GfxFormat format)
        : width(width), height(height), depth(depth), format(format), multiSampling(MultiSampling::Sample_Count_1),
          mipLevels(1), isCubemap(false)
    {}

    float3 GetSize() const { return {width, height, depth}; }

    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth = 1;
    uint32_t layers = 1;
    Gfx::GfxFormat format = GfxFormat::R8G8B8A8_SRGB;
    Gfx::MultiSampling multiSampling = MultiSampling::Sample_Count_1;
    uint32_t mipLevels = 1;
    bool isCubemap = false;

    uint32_t GetLayer() const { return isCubemap ? 6 : layers; }

    size_t GetByteSize() const { return CalcByteSize(); }

private:
    size_t CalcByteSize() const
    {
        size_t pixels = 0;
        float scale = 1.0f;
        for (int i = 0; i < mipLevels; i++)
        {
            int lw = width * scale;
            int lh = height * scale;

            pixels += lw * lh;
            scale *= 0.5;
        }
        return pixels * GetLayer() * MapGfxFormatToByteSize(format);
    }
};
} // namespace Gfx
