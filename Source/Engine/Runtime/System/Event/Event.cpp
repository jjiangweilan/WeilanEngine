#include "Event.hpp"
#include <SDL.h>
#if ENGINE_EDITOR
#include "Engine/ThirdParty/imgui/imgui_impl_sdl2.h"
#endif
#include <spdlog/spdlog.h>

void Event::Init()
{
    Input::Init();
}
void Event::Deinit()
{
    Input::Destroy();
}

void Event::Reset()
{
    windowSizeChange.state = false;
    windowClose.state = false;
}
void Event::Poll()
{

    SDL_Event event;
    Input::Reset();
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
        Input::PushEvent(event);
    }
    Input::UpdateState();
}
