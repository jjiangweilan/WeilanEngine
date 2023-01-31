#include "VKAppWindow.hpp"
#include <iostream>
#include <spdlog/spdlog.h>

namespace Engine::Gfx
{
VKAppWindow::VKAppWindow(Extent2D windowSize) : windowSize(windowSize)
{
    if (!SDL_WasInit(SDL_INIT_VIDEO))
        SDL_InitSubSystem(SDL_INIT_VIDEO);

    SDL_DisplayMode displayMode;
    // MacOS return points not pixels
    SDL_GetCurrentDisplayMode(0, &displayMode);

    if (windowSize.width > displayMode.w)
        windowSize.width = displayMode.w * 0.8;
    if (windowSize.height > displayMode.h)
        windowSize.height = displayMode.h * 0.8;

    window = SDL_CreateWindow("WeilanGame",
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              windowSize.width,
                              windowSize.height,
                              SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

    int drawableWidth, drawbaleHeight;
    SDL_GL_GetDrawableSize(window, &drawableWidth, &drawbaleHeight);

    windowSize.width = drawableWidth;
    windowSize.height = drawbaleHeight;

    // the window size maybe restricted by the system when the first time creating it (e.g. MacOS)
    // so we immediately query the actual size after we create the window
    // int width = windowSize.width;
    // int height = windowSize.height;
    // SDL_GetWindowSize(window, &width, &height);
}

VKAppWindow::~VKAppWindow() { SDL_QuitSubSystem(SDL_INIT_VIDEO); }

void VKAppWindow::CreateVkSurface(VkInstance vkInstance, VkSurfaceKHR* vkSurface)
{
    if (!SDL_Vulkan_CreateSurface(window, vkInstance, vkSurface))
    {
        spdlog::critical("Window surface creation failed: {0}", SDL_GetError());
    }
}

std::vector<const char*> VKAppWindow::GetVkRequiredExtensions()
{
    unsigned int count;
    if (!SDL_Vulkan_GetInstanceExtensions(window, &count, nullptr))
    {
        std::cout << SDL_GetError() << std::endl;
        std::cout << "GetVkRequiredExtensions failed" << std::endl;
    }

    std::vector<const char*> names(count);
    if (!SDL_Vulkan_GetInstanceExtensions(window, &count, names.data()))
    {
        std::cout << SDL_GetError() << std::endl;
        std::cout << "GetVkRequiredExtensions failed" << std::endl;
    }

    return names;
}
} // namespace Engine::Gfx
