#pragma once

#include "Engine/Core/Asset.hpp"
#include <cstdint>
#include <vector>

struct CursorAtlasHotspot
{
    uint32_t x = 0;
    uint32_t y = 0;

    INLINE_DEFINE_SERIALIZABLE(
        SER(x),
        SER(y)
    )
};

class CursorAtlas : public Asset
{
    DECLARE_ASSET();
    DECLARE_SERIALIZATION();

public:
    static constexpr uint32_t BytesPerPixel = 4;

    uint32_t atlasWidth = 0;
    uint32_t atlasHeight = 0;
    uint32_t frameWidth = 32;
    uint32_t frameHeight = 32;
    uint32_t columns = 1;
    uint32_t rows = 1;
    std::vector<uint32_t> rgbaPixels;
    std::vector<CursorAtlasHotspot> hotspots;

    int GetCursorCount() const;
    bool GetCursorPixels(
        int textureIndex,
        std::vector<uint8_t>& outPixels,
        uint32_t& width,
        uint32_t& height,
        uint32_t& hotspotX,
        uint32_t& hotspotY
    ) const;
};
