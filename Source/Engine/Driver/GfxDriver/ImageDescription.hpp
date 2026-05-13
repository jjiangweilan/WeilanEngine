#pragma once
#include "GfxEnums.hpp"
#include "Engine/Library/Math.hpp"

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

    size_t GetMipByteSize(uint32_t mipLevel) const
    {
        uint32_t mipWidth = width >> mipLevel;
        uint32_t mipHeight = height >> mipLevel;
        uint32_t mipDepth = depth >> mipLevel;
        mipWidth = mipWidth > 0 ? mipWidth : 1;
        mipHeight = mipHeight > 0 ? mipHeight : 1;
        mipDepth = mipDepth > 0 ? mipDepth : 1;

        if (IsCompressedFormat(format))
        {
            const uint32_t blockWidth = MapGfxFormatToBlockWidth(format);
            const uint32_t blockHeight = MapGfxFormatToBlockHeight(format);
            const size_t blockCountX = (mipWidth + blockWidth - 1) / blockWidth;
            const size_t blockCountY = (mipHeight + blockHeight - 1) / blockHeight;
            return blockCountX * blockCountY * mipDepth * GetLayer() * MapGfxFormatToBlockByteSize(format);
        }

        return static_cast<size_t>(mipWidth) * mipHeight * mipDepth * GetLayer() * MapGfxFormatToByteSize(format);
    }

private:
    size_t CalcByteSize() const
    {
        size_t byteSize = 0;
        for (uint32_t i = 0; i < mipLevels; i++)
        {
            byteSize += GetMipByteSize(i);
        }
        return byteSize;
    }
};
} // namespace Gfx
