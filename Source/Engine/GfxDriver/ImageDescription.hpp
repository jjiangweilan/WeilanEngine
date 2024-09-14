#pragma once
#include "GfxEnums.hpp"

namespace Gfx
{

struct ImageDescription
{
    ImageDescription() = default;
    ImageDescription(
        uint32_t width,
        uint32_t height,
        uint32_t depth,
        Gfx::ImageFormat format,
        Gfx::MultiSampling multiSampling,
        uint32_t mipLevels,
        bool isCubemap
    )
        : width(width), height(height), depth(depth), format(format), multiSampling(multiSampling),
          mipLevels(mipLevels), isCubemap(isCubemap)
    {}

    ImageDescription(uint32_t width, uint32_t height, Gfx::ImageFormat format)
        : width(width), height(height), depth(1.0f), format(format), multiSampling(MultiSampling::Sample_Count_1),
          mipLevels(1), isCubemap(false)
    {}

    ImageDescription(uint32_t width, uint32_t height, uint32_t depth, Gfx::ImageFormat format)
        : width(width), height(height), depth(depth), format(format), multiSampling(MultiSampling::Sample_Count_1),
          mipLevels(1), isCubemap(false)
    {}

    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth = 1;
    Gfx::ImageFormat format = ImageFormat::R8G8B8A8_SRGB;
    Gfx::MultiSampling multiSampling = MultiSampling::Sample_Count_1;
    uint32_t mipLevels = 1;
    bool isCubemap = false;

    uint32_t GetLayer() const
    {
        return isCubemap ? 6 : 1;
    }

    uint32_t GetByteSize() const
    {
        return width * height * MapImageFormatToByteSize(format) * (isCubemap ? 6 : 1);
    }
};
} // namespace Gfx
