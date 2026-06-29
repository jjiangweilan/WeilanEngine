#pragma once

#include "Engine/Game/Input.hpp"
#include <SDL_gamecontroller.h>
#include <SDL_keyboard.h>
#include <SDL_mouse.h>
#include <SDL_video.h>

struct WindowSizeChange
{
    bool state;
    int width;
    int height;
};

struct WindowClose
{
    bool state;
};

struct SwapchainRecreated
{
    bool state;
};

class Event
{
public:
    void Init(SDL_Window* mainWindow);
    void Deinit();
    void Poll();
    void Reset();
    void SetMainWindow(SDL_Window* window);

    const WindowSizeChange& GetWindowSizeChanged()
    {
        return windowSizeChange;
    }
    const WindowClose& GetWindowClose()
    {
        return windowClose;
    }
    const SwapchainRecreated& GetSwapchainRecreated()
    {
        return swapchainRecreated;
    }

private:
    SDL_Window* mainWindow = nullptr;
    Uint32 mainWindowId = 0;
    WindowSizeChange windowSizeChange;
    WindowClose windowClose;
    SwapchainRecreated swapchainRecreated;

    friend class WeilanEngine;
};
