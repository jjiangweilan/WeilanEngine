#pragma once
#include "GfxEnums.hpp"

namespace Engine::Gfx
{
struct ImageDescription
{
    uint32_t width;
    uint32_t height;
    Gfx::ImageFormat format;
    Gfx::MultiSampling multiSampling;
    uint32_t mipLevels;
    bool isCubemap;
};
} // namespace Engine::Gfx
