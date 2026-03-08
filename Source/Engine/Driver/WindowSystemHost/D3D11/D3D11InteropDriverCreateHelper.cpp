#include "D3D11InteropDriverCreateHelper.hpp"
#include "Engine/Driver/WindowSystemHost/D3D11/D3D11InteropDriver.hpp"

std::unique_ptr<WindowSystemHost::IInteropDriver> CreateD3D11InteropDriver()
{
    return std::make_unique<WindowSystemHost::D3D11InteropDriver>();
}

void* WeilanEngine_CreateWindow()
{
    HINSTANCE hInst = GetModuleHandle(NULL);

    WNDCLASS wc = {};
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hInstance = hInst;
    wc.lpszClassName = "window";
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

            // If the mouse is inside the client area, tell Windows it's the caption/title bar
            if (hit == HTCLIENT)
            {
                return HTCAPTION;
            }
            return hit;
        }
        return DefWindowProc(window, message, wparam, lparam);
    };

    RegisterClass(&wc);

    HWND const window = CreateWindowEx(WS_EX_NOREDIRECTIONBITMAP, wc.lpszClassName, "Sample", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr, hInst, nullptr);

    LONG_PTR style = GetWindowLongPtr(window, GWL_STYLE);
    style &= ~(WS_BORDER | WS_CAPTION | WS_THICKFRAME);
    SetWindowLongPtr(window, GWL_STYLE, style);

    return (void*)window;
}
