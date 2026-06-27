#include "Cursor.hpp"

#include "Engine/Library/Platforms/Cursor.hpp"
#include "Engine/Runtime/Object/Texture/CursorAtlas.hpp"

bool Cursor::SetCursorAppearance(CursorAtlas* atlas, int textureIndex)
{
    if (atlas == nullptr)
    {
        ResetCursorAppearance();
        return true;
    }

    std::vector<uint8_t> pixels;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t hotspotX = 0;
    uint32_t hotspotY = 0;
    if (!atlas->GetCursorPixels(textureIndex, pixels, width, height, hotspotX, hotspotY))
    {
        return false;
    }

    return Platform_SetCursorAppearance(pixels.data(), width, height, hotspotX, hotspotY);
}

void Cursor::ResetCursorAppearance()
{
    Platform_ResetCursorAppearance();
}
