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
    void Present() override{
        // do nothing, present is managed by vkdriver and is done every frame
    };

    Swapchain swapchain;
    Surface surface;
    int swapchainCount;
    uint32_t swapchainIndex;
    VkSemaphore imageAcquireSemaphore;
    VkSemaphore presentSemaphore;
};
} // namespace Gfx
