#include "VKSwapchain.hpp"
#include "SDL_vulkan.h"
#include <spdlog/spdlog.h>

namespace Gfx
{
VKWindow::VKWindow(SDL_Window* window)
{
    VkInstance instance = VKContext::Instance()->instance;
    if (!SDL_Vulkan_CreateSurface(window, instance, &surface.handle))
    {
        spdlog::critical("Window surface creation failed: {0}", SDL_GetError());
    }

    surface.QuerySurfaceProperties(VKContext::Instance()->gpu->handle);
}

Image* VKWindow::GetSwapchainImage() {}
void VKWindow::SetSurfaceSize(int width, int height) {}
void VKWindow::Present() {}
} // namespace Gfx
