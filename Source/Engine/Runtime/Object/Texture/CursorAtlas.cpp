#include "CursorAtlas.hpp"

#include <algorithm>

DEFINE_ASSET(CursorAtlas, "3013F5FA-7E22-410D-A2C8-721F02D121B0", "cursorAtlas")

DEFINE_SERIALIZATION(
    CursorAtlas,
    Asset,
    SER(atlasWidth),
    SER(atlasHeight),
    SER(frameWidth),
    SER(frameHeight),
    SER(columns),
    SER(rows),
    SER(rgbaPixels),
    SER(hotspots)
)

int CursorAtlas::GetCursorCount() const
{
    return static_cast<int>(columns * rows);
}

bool CursorAtlas::GetCursorPixels(
    int textureIndex,
    std::vector<uint8_t>& outPixels,
    uint32_t& width,
    uint32_t& height,
    uint32_t& hotspotX,
    uint32_t& hotspotY
) const
{
    if (textureIndex < 0 || textureIndex >= GetCursorCount() || atlasWidth == 0 || atlasHeight == 0 ||
        frameWidth == 0 || frameHeight == 0 || columns == 0 || rows == 0)
    {
        return false;
    }

    const uint64_t expectedPixelCount = static_cast<uint64_t>(atlasWidth) * atlasHeight;
    if (rgbaPixels.size() < expectedPixelCount)
    {
        return false;
    }

    const uint32_t frameX = static_cast<uint32_t>(textureIndex) % columns;
    const uint32_t frameY = static_cast<uint32_t>(textureIndex) / columns;
    const uint32_t srcX = frameX * frameWidth;
    const uint32_t srcY = frameY * frameHeight;
    if (srcX + frameWidth > atlasWidth || srcY + frameHeight > atlasHeight)
    {
        return false;
    }

    width = frameWidth;
    height = frameHeight;
    hotspotX = 0;
    hotspotY = 0;
    if (textureIndex < static_cast<int>(hotspots.size()))
    {
        hotspotX = std::min(hotspots[textureIndex].x, frameWidth - 1);
        hotspotY = std::min(hotspots[textureIndex].y, frameHeight - 1);
    }

    outPixels.resize(static_cast<size_t>(frameWidth) * frameHeight * BytesPerPixel);
    for (uint32_t y = 0; y < frameHeight; ++y)
    {
        for (uint32_t x = 0; x < frameWidth; ++x)
        {
            const uint32_t packed = rgbaPixels[(srcY + y) * atlasWidth + srcX + x];
            const size_t dst = (static_cast<size_t>(y) * frameWidth + x) * BytesPerPixel;
            outPixels[dst + 0] = static_cast<uint8_t>(packed & 0xFF);
            outPixels[dst + 1] = static_cast<uint8_t>((packed >> 8) & 0xFF);
            outPixels[dst + 2] = static_cast<uint8_t>((packed >> 16) & 0xFF);
            outPixels[dst + 3] = static_cast<uint8_t>((packed >> 24) & 0xFF);
        }
    }

    return true;
}
