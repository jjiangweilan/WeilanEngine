#include "TransparentWindowPixel.hpp"
#include <dwmapi.h>

#include <d3d11.h>
#include <dcomp.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

ComPtr<IDCompositionDevice> dcompDevice;
ComPtr<IDCompositionTarget> dcompTarget;
ComPtr<IDCompositionVisual> dcompVisual;

bool InitDirectComposition(HWND hwnd, IUnknown* swapChain)
{

    return true;
}

void TransparentWindowPixel::EnableTransparent(SDL_Window* window)
{
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(window, &wmInfo);
    HWND hwnd = wmInfo.info.win.window;

    // Set window styles for transparency and DirectComposition
    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    exStyle = WS_EX_TOPMOST | WS_EX_LAYERED;
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);

    // Modern way to enable transparency for DWM
    // MARGINS margins = {-1, -1, -1, -1};
    // DwmExtendFrameIntoClientArea(hwnd, &margins);
    //
    // DWM_BLURBEHIND bb = {0};
    // bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
    // bb.fEnable = TRUE;
    // bb.hRgnBlur = CreateRectRgn(0, 0, -1, -1); // Makes the whole window "glass"
    // DwmEnableBlurBehindWindow(hwnd, &bb);

    // Force style update
    // SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
}

void TransparentWindowPixel::ForceTopmost(SDL_Window* window)
{
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(window, &wmInfo);
    HWND hwnd = wmInfo.info.win.window;

    // UpdateLayeredWindow(hwnd, nullptr, nullptr, nullptr, nullptr, 0, nullptr, nullptr, ULW_ALPHA);
}
