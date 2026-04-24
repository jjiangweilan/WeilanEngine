#pragma once
#include <SDL2/SDL_syswm.h>

class TransparentWindowPixel
{
public:
    static void ForceTopmost(SDL_Window* window);
    static void EnableTransparent(SDL_Window* window);
};
