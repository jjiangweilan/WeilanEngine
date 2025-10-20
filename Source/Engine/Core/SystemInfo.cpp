#include "SystemInfo.hpp"
#include <SDL2/SDL_video.h>

SystemInfo& SystemInfo::Singleton()
{
    static SystemInfo i;
    return i;
}

int2 SystemInfo::GetDisplaySize()
{
    SDL_DisplayMode mode;
    SDL_GetCurrentDisplayMode(0, &mode);

    return {mode.w, mode.h};
}
