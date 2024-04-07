#pragma once
#include "../Window.hpp"
#include "GfxDriver/Vulkan/VKContext.hpp"
#include "SDL_video.h"
#include "vulkan/vulkan_core.h"

namespace Gfx
{
class VKWindow : public Window
{
    VKWindow(SDL_Window* window);
    Image* GetSwapchainImage() override;
    void SetSurfaceSize(int width, int height) override;
    void Present() override;

private:
    Surface surface;
};
} // namespace Gfx
