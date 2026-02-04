#include "SystemInfo.hpp"
#include <SDL2/SDL_video.h>

SystemInfo& SystemInfo::Singleton()
{
    static SystemInfo i;
    return i;
}

int2 SystemInfo::GetSystemWindowSize()
{
    SDL_DisplayMode mode;
    SDL_GetCurrentDisplayMode(0, &mode);

    return {mode.w, mode.h};
}

int2 SystemInfo::GetGameViewOrigin()
{
    return gameViewOrigin;
}

void SystemInfo::SetGameViewOrigin(int2 origin)
{
    this->gameViewOrigin = origin;
}

void SystemInfo::GetScreenSize(float& width, float& height)
{
    width = screenWidth;
    height = screenHeight;
};

void SystemInfo::SetScreenSize(float width, float height)
{
    this->screenWidth = width;
    this->screenHeight = height;
}
