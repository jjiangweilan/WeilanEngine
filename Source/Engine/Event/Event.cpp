#include "Event.hpp"
#include <SDL.h>
#if ENGINE_EDITOR
#include "ThirdParty/imgui/imgui_impl_sdl2.h"
#endif

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
}
