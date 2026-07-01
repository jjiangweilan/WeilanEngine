#include "D3D11InteropDriverCreateHelper.hpp"
#include "Engine/Driver/WindowSystemHost/D3D11/D3D11InteropDriver.hpp"
#include <stdexcept>
#include <windowsx.h>

namespace
{
constexpr const char* InteropWindowClassName = "WeilanEngineD3D11InteropWindow";

WindowSystemHost::IInteropDriver* GetHitTestDriver(HWND window)
{
    return reinterpret_cast<WindowSystemHost::IInteropDriver*>(GetWindowLongPtr(window, GWLP_USERDATA));
}
} // namespace

std::unique_ptr<WindowSystemHost::IInteropDriver> CreateD3D11InteropDriver()
{
    return std::make_unique<WindowSystemHost::D3D11InteropDriver>();
}

void* WeilanEngine_CreateWindow(int32_t x, int32_t y, uint32_t width, uint32_t height)
{
    HINSTANCE hInst = GetModuleHandle(NULL);

    WNDCLASS wc = {};
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hInstance = hInst;
    wc.lpszClassName = InteropWindowClassName;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc =
        [](HWND window, UINT message, WPARAM wparam, LPARAM lparam) -> LRESULT
    {
        // if (WM_DESTROY == message)
        // {
        //     PostQuitMessage(0);
        //     return 0;
        // }
        // return DefWindowProc(window, message, wparam, lparam);

        // TEMP code to make the window movable, we will handle the input in a better way later
        if (WM_DESTROY == message)
        {
            PostQuitMessage(0);
            return 0;
        }
        else if (WM_NCHITTEST == message)
        {
            // First, let the default procedure determine where the mouse is
            LRESULT hit = DefWindowProc(window, message, wparam, lparam);

            if (hit == HTCLIENT)
            {
                POINT screenPos = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                POINT clientPos = screenPos;
                ScreenToClient(window, &clientPos);

                if (auto* driver = GetHitTestDriver(window))
                {
                    if (!driver->HitTestVisible(clientPos.x, clientPos.y))
                    {
                        return HTTRANSPARENT;
                    }
                }

                return HTCLIENT;
            }
            return hit;
        }
        return DefWindowProc(window, message, wparam, lparam);
    };

    if (!RegisterClass(&wc))
    {
        DWORD error = GetLastError();
        if (error != ERROR_CLASS_ALREADY_EXISTS)
            throw std::runtime_error("Failed to register D3D11 interop window class");
    }

    HWND const window = CreateWindowEx(WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOPMOST | WS_EX_LAYERED, wc.lpszClassName, "Sample", WS_OVERLAPPEDWINDOW | WS_VISIBLE, x, y, static_cast<int>(width), static_cast<int>(height), nullptr, nullptr, hInst, nullptr);
    if (!window)
        throw std::runtime_error("Failed to create D3D11 interop window");

    LONG_PTR style = GetWindowLongPtr(window, GWL_STYLE);
    style &= ~(WS_BORDER | WS_CAPTION | WS_THICKFRAME);
    SetWindowLongPtr(window, GWL_STYLE, style);

    return (void*)window;
}

void WeilanEngine_SetWindowBounds(void* windowHandle, int32_t x, int32_t y, uint32_t width, uint32_t height)
{
    SetWindowPos((HWND)windowHandle, nullptr, x, y, static_cast<int>(width), static_cast<int>(height), SWP_NOZORDER | SWP_NOACTIVATE);
}

void WeilanEngine_DestroyWindow(void* windowHandle)
{
    DestroyWindow((HWND)windowHandle);
}

void WeilanEngine_SetWindowHitTestDriver(void* windowHandle, WindowSystemHost::IInteropDriver* driver)
{
    SetWindowLongPtr((HWND)windowHandle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(driver));
}
