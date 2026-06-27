#if _WIN64
#include "WindowsCursor.hpp"

#include <Windows.h>
#include <vector>

namespace
{
HCURSOR customCursor = nullptr;

void DestroyCustomCursor()
{
    if (customCursor != nullptr)
    {
        DestroyCursor(customCursor);
        customCursor = nullptr;
    }
}
} // namespace

bool Platform_SetCursorAppearance(
    const uint8_t* rgba,
    uint32_t width,
    uint32_t height,
    uint32_t hotspotX,
    uint32_t hotspotY
)
{
    if (rgba == nullptr || width == 0 || height == 0)
    {
        return false;
    }

    std::vector<uint8_t> bgra(static_cast<size_t>(width) * height * 4);
    for (uint32_t y = 0; y < height; ++y)
    {
        for (uint32_t x = 0; x < width; ++x)
        {
            const size_t src = (static_cast<size_t>(y) * width + x) * 4;
            const size_t dst = src;
            bgra[dst + 0] = rgba[src + 2];
            bgra[dst + 1] = rgba[src + 1];
            bgra[dst + 2] = rgba[src + 0];
            bgra[dst + 3] = rgba[src + 3];
        }
    }

    BITMAPV5HEADER bitmapHeader{};
    bitmapHeader.bV5Size = sizeof(BITMAPV5HEADER);
    bitmapHeader.bV5Width = static_cast<LONG>(width);
    bitmapHeader.bV5Height = -static_cast<LONG>(height);
    bitmapHeader.bV5Planes = 1;
    bitmapHeader.bV5BitCount = 32;
    bitmapHeader.bV5Compression = BI_BITFIELDS;
    bitmapHeader.bV5RedMask = 0x00FF0000;
    bitmapHeader.bV5GreenMask = 0x0000FF00;
    bitmapHeader.bV5BlueMask = 0x000000FF;
    bitmapHeader.bV5AlphaMask = 0xFF000000;

    HDC screenDC = GetDC(nullptr);
    void* bitmapData = nullptr;
    HBITMAP colorBitmap = CreateDIBSection(
        screenDC,
        reinterpret_cast<BITMAPINFO*>(&bitmapHeader),
        DIB_RGB_COLORS,
        &bitmapData,
        nullptr,
        0
    );
    ReleaseDC(nullptr, screenDC);

    if (colorBitmap == nullptr || bitmapData == nullptr)
    {
        return false;
    }

    memcpy(bitmapData, bgra.data(), bgra.size());

    HBITMAP maskBitmap = CreateBitmap(width, height, 1, 1, nullptr);
    if (maskBitmap == nullptr)
    {
        DeleteObject(colorBitmap);
        return false;
    }

    ICONINFO iconInfo{};
    iconInfo.fIcon = FALSE;
    iconInfo.xHotspot = hotspotX;
    iconInfo.yHotspot = hotspotY;
    iconInfo.hbmMask = maskBitmap;
    iconInfo.hbmColor = colorBitmap;

    HCURSOR newCursor = CreateIconIndirect(&iconInfo);
    DeleteObject(colorBitmap);
    DeleteObject(maskBitmap);

    if (newCursor == nullptr)
    {
        return false;
    }

    DestroyCustomCursor();
    customCursor = newCursor;
    SetCursor(customCursor);
    return true;
}

void Platform_ResetCursorAppearance()
{
    SetCursor(LoadCursor(nullptr, IDC_ARROW));
    DestroyCustomCursor();
}

#endif
