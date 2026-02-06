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

void TransparentWindowPixel::EnableTransparent(SDL_Window* window, void* swapchain)
{
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(window, &wmInfo);
    HWND hwnd = wmInfo.info.win.window;

    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    exStyle |= WS_EX_TOPMOST | WS_EX_NOREDIRECTIONBITMAP;

    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);

    ComPtr<ID3D11Device> d3d11Device;
    D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, &d3d11Device, nullptr, nullptr);

    if (FAILED(DCompositionCreateDevice(nullptr, __uuidof(IDCompositionDevice), (void**)&dcompDevice)))
    {
        return;
    }

    if (FAILED(dcompDevice->CreateTargetForHwnd(hwnd, TRUE, &dcompTarget)))
    {
        return;
    }

    dcompDevice->CreateVisual(&dcompVisual);

    dcompVisual->SetContent((IUnknown*)swapchain);

    dcompTarget->SetRoot(dcompVisual.Get());
    dcompDevice->Commit();
}

void TransparentWindowPixel::ForceTopmost(SDL_Window* window)
{
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(window, &wmInfo);
    HWND hwnd = wmInfo.info.win.window;

    // SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);

    DWM_BLURBEHIND bb = {0};
    bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
    bb.fEnable = TRUE;
    bb.hRgnBlur = CreateRectRgn(0, 0, -1, -1);
    DwmEnableBlurBehindWindow(hwnd, &bb);
}
