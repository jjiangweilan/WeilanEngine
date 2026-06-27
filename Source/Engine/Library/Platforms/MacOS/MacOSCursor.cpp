#if __APPLE__
#include "MacOSCursor.hpp"

bool Platform_SetCursorAppearance(
    const uint8_t* rgba,
    uint32_t width,
    uint32_t height,
    uint32_t hotspotX,
    uint32_t hotspotY
)
{
    return false;
}

void Platform_ResetCursorAppearance() {}

#endif
