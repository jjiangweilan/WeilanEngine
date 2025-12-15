#pragma once
#include "Exporter.hpp"
#include "Driver/GfxDriver/GfxEnums.hpp"
#include <cinttypes>
#include <cstddef>

namespace Exporters
{
class KtxExporter : Exporter
{
public:
    // [layer [face [mip]]]
    static void Export(
        const char* path,
        uint8_t* src,
        uint32_t width,
        uint32_t height,
        uint32_t depth,
        uint32_t dimension,
        uint32_t layer,
        uint32_t mipLevel,
        bool isArray,
        bool isCubemap,
        Gfx::GfxFormat format,
        bool enableCompression = false
    );
};
} // namespace Exporters
