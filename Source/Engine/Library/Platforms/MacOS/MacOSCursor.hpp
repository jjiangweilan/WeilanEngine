#pragma once

#if __APPLE__
#include <cstdint>

bool Platform_SetCursorAppearance(
    const uint8_t* rgba,
    uint32_t width,
    uint32_t height,
    uint32_t hotspotX,
    uint32_t hotspotY
);
void Platform_ResetCursorAppearance();

#endif
