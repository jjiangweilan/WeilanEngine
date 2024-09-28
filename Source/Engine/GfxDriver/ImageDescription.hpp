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
    {
        CalcByteSize();
    }

    ImageDescription(uint32_t width, uint32_t height, Gfx::ImageFormat format)
        : width(width), height(height), depth(1.0f), format(format), multiSampling(MultiSampling::Sample_Count_1),
          mipLevels(1), isCubemap(false)
    {
        CalcByteSize();
    }

    ImageDescription(uint32_t width, uint32_t height, uint32_t depth, Gfx::ImageFormat format)
        : width(width), height(height), depth(depth), format(format), multiSampling(MultiSampling::Sample_Count_1),
          mipLevels(1), isCubemap(false)
    {
        CalcByteSize();
    }

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
        return byteSize;
    }

private:
    void CalcByteSize()
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
        byteSize = pixels * GetLayer() * MapImageFormatToByteSize(format);
    }
    size_t byteSize = 0;
};
} // namespace Gfx
