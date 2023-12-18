#pragma once
#include "GfxEnums.hpp"

namespace Engine::Gfx
{

enum class ImageType
{
    Texture,
    Dynamic
};

struct ImageDescription
{
    uint32_t width;
    uint32_t height;
    Gfx::ImageFormat format;
    Gfx::MultiSampling multiSampling;
    uint32_t mipLevels;
    bool isCubemap;
    uint32_t GetLayer() const
    {
        return isCubemap ? 6 : 1;
    }
    uint32_t GetByteSize() const
    {
        return width * height * MapImageFormatToByteSize(format) * (isCubemap ? 6 : 1);
    }
};
} // namespace Engine::Gfx
