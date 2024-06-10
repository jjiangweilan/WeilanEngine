#pragma once

#include "Gameplay/Input.hpp"
#include <SDL_gamecontroller.h>
#include <SDL_keyboard.h>
#include <SDL_mouse.h>

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
    void Init();
    void Deinit();
    void Poll();
    void Reset();

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
    WindowSizeChange windowSizeChange;
    WindowClose windowClose;
    SwapchainRecreated swapchainRecreated;

    SDL_GameController* gameController;

    friend class WeilanEngine;
};
