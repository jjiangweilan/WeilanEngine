#pragma once
#include "GfxEnums.hpp"

namespace Engine::Gfx
{
    struct ImageDescription
    {
        unsigned char*          data = nullptr;
        uint32_t                width = 0;
        uint32_t                height = 0;
        Gfx::ImageFormat        format = Gfx::ImageFormat::R8G8B8A8_UNorm;
        Gfx::MultiSampling      multiSampling = Gfx::MultiSampling::Sample_Count_1;
        uint32_t                mipLevels = 1;
    };
}