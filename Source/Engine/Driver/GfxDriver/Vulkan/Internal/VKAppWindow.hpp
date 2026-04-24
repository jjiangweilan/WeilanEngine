#pragma once

#include "Engine/Runtime/System/Rendering/Structs.hpp"
#include <SDL.h>
#include <SDL_vulkan.h>
#include <glm/glm.hpp>
#include "Engine/Library/DynamicArray.hpp"
#include "Engine/Driver/GfxDriver/Vulkan/VKCommon.hpp"

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
