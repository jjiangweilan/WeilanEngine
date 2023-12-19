#pragma once
#include "Exporter.hpp"
#include "GfxDriver/GfxEnums.hpp"
#include <cinttypes>
#include <cstddef>

namespace Exporters
{
class KtxExporter : Exporter
{
public:
    static void Export(
        const char* path,
        uint8_t* src,
        size_t srcSize,
        uint32_t width,
        uint32_t height,
        uint32_t depth,
        uint32_t dimension,
        uint32_t layer,
        uint32_t mipLevel,
        bool isArray,
        bool isCubemap,
        Gfx::ImageFormat format
    );
};
} // namespace Exporters
