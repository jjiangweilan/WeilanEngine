#pragma once
#include "../Window.hpp"
#include "ThirdParty/imgui/imgui_impl_sdl2.h"
#include "VKContext.hpp"

namespace Gfx
{
class Image;

// data structure for extra windows
class VKWindow : public Window
{
public:
    VKWindow(SDL_Window* window, int swapchainCount);
    ~VKWindow() override;

    Image* GetSwapchainImage() override;
    void SetSurfaceSize(int width, int height) override;
    void SetSurfaceSizeImple(int width, int height);
    void Present() override
    {
        presentRequest.requested = true;
    };

    struct
    {
        bool requested = false;
    } presentRequest;

    SDL_Window* window;
    Swapchain swapchain;
    Surface surface;
    int swapchainCount;
    uint32_t swapchainIndex;
    uint32_t activeIndex;
    std::vector<VkSemaphore> imageAcquireSemaphores;
    std::vector<VkSemaphore> presentSemaphores;
};
} // namespace Gfx
