#pragma once
#include <cstdint>

namespace WindowSystemHost
{
    struct WindowPosition
    {
        int32_t x;
        int32_t y;
    };

    class IInteropDriver
    {
    public:
        virtual ~IInteropDriver() = default;

        // Initializes the interop driver with the target window and dimensions.
        // windowHandle is expected to be an HWND on Windows.
        virtual void Initialize(void* windowHandle, uint32_t width, uint32_t height) = 0;

        // Resizes the underlying swapchain/buffers
        virtual void Resize(uint32_t width, uint32_t height) = 0;

        // Moves the target window to the specified virtual-desktop coordinates.
        virtual void SetWindowPosition(int32_t x, int32_t y) = 0;

        // Returns the target window position in virtual-desktop coordinates.
        virtual WindowPosition GetWindowPosition() const = 0;

        // Returns the shared NT handle for the texture/swapchain backbuffer
        virtual void* GetSharedHandle() = 0;

        // Returns the shared NT handle for the synchronization fence
        virtual void* GetFenceHandle() = 0;

        // Wait on the fence for the specified value (GPU wait)
        virtual void Wait(uint64_t value) = 0;

        // Signal the fence with the specified value (GPU signal)
        virtual void Signal(uint64_t value) = 0;

        // Commits the visual tree or presents the swapchain
        virtual void Present() = 0;

        // Returns whether the cached presented alpha says this client pixel should receive mouse input.
        virtual bool HitTestVisible(int clientX, int clientY) const { return true; }
    };
}
