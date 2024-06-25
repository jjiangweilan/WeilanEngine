#pragma once

#include "Rendering/Structs.hpp"
#include <SDL.h>
#include <SDL_vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <vulkan/vulkan.h>

namespace Gfx
{
class VKAppWindow
{
public:
    VKAppWindow(Extent2D windowSize);
    ~VKAppWindow();

    void CreateVkSurface(VkInstance vkInstance, VkSurfaceKHR* vkSurface);
    std::vector<const char*> GetVkRequiredExtensions();

    SDL_Window* GetSDLWindow()
    {
        return window;
    }

    Extent2D GetDefaultWindowSize()
    {
        return windowSize;
    }

private:
    SDL_Window* window;
    Extent2D windowSize = {1920, 1080};

    friend class GfxContext;
};

} // namespace Gfx
