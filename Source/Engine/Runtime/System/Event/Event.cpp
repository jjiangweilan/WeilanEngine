#include "Event.hpp"
#include "Engine/Runtime/System/UserInterface/UI.hpp"
#include <SDL.h>
#if ENGINE_EDITOR
#include "Engine/ThirdParty/imgui/imgui_impl_sdl2.h"
#endif
#include <spdlog/spdlog.h>

void Event::Init(SDL_Window* mainWindow)
{
    SetMainWindow(mainWindow);
}
void Event::Deinit()
{
}

void Event::Reset()
{
    windowSizeChange.state = false;
    windowClose.state = false;
}

void Event::SetMainWindow(SDL_Window* window)
{
    mainWindow = window;
    mainWindowId = mainWindow ? SDL_GetWindowID(mainWindow) : 0;
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

        bool uiConsumeMouseInput = UI::ProcessSDLEvent(event);

        if (event.type == SDL_WINDOWEVENT)
        {
            if (mainWindowId != 0 && event.window.windowID != mainWindowId)
            {
                Input::PushEvent(event, uiConsumeMouseInput);
                continue;
            }

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

        Input::PushEvent(event, uiConsumeMouseInput);
    }
    Input::UpdateState();
}
