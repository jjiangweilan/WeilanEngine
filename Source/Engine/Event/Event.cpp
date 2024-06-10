#include "Event.hpp"
#include "Gameplay/Input.hpp"
#include <SDL.h>
#if ENGINE_EDITOR
#include "ThirdParty/imgui/imgui_impl_sdl2.h"
#endif
#include <spdlog/spdlog.h>

void Event::Init()
{
    gameController = SDL_GameControllerOpen(0);
}
void Event::Deinit()
{
    SDL_GameControllerClose(gameController);
}

void Event::Reset()
{
    windowSizeChange.state = false;
    windowClose.state = false;
}
void Event::Poll()
{

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
#if ENGINE_EDITOR
        ImGui_ImplSDL2_ProcessEvent(&event);
#endif

        if (event.type == SDL_WINDOWEVENT)
        {
            if (event.window.event == SDL_WINDOWEVENT_CLOSE)
            {
                windowClose.state = true;
            }
            if (event.window.event == SDL_WINDOWEVENT_RESIZED)
            {
                windowSizeChange.state = true;
                windowSizeChange.width = event.window.data1;
                windowSizeChange.height = event.window.data2;
            }
        }
        Input::GetSingleton().PushEvent(event);
    }
}
