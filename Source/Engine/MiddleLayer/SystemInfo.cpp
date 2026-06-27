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

float2 SystemInfo::GetScreenResolution()
{
    auto& systemInfo = Singleton();
    return {systemInfo.screenResolutionWidth, systemInfo.screenResolutionHeight};
}

float2 SystemInfo::GetScreenSize()
{
    auto& systemInfo = Singleton();
    return {systemInfo.screenWidth, systemInfo.screenHeight};
}

void SystemInfo::SetScreenResolution(float width, float height)
{
    this->screenResolutionWidth = width;
    this->screenResolutionHeight = height;
}

void SystemInfo::SetScreenSize(float width, float height)
{
    this->screenWidth = width;
    this->screenHeight = height;
}
