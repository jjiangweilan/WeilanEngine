#include "Input.hpp"
#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

Input::Input() {}
Input& Input::GetSingleton()
{
    static Input input;
    return input;
}

void Input::PushEvent(SDL_Event& event)
{
    if (event.type == SDL_JOYAXISMOTION)
    {
        if (event.jaxis.axis == 0) // left x
        {
            int val = static_cast<int>(event.jaxis.value);
            leftJoyAxis.x = JoyStickRemap(val);
        }
        else if (event.jaxis.axis == 1) // left y
        {
            int val = -static_cast<int>(event.jaxis.value);
            leftJoyAxis.y = JoyStickRemap(val);
        }
        else if (event.jaxis.axis == 2) // right x
        {
            int val = static_cast<int>(event.jaxis.value);
            rightJoyAxis.x = JoyStickRemap(val);
        }
        else if (event.jaxis.axis == 3) // right y
        {
            int val = -static_cast<int>(event.jaxis.value);
            rightJoyAxis.y = JoyStickRemap(val);
        }
    }
    else if (event.type == SDL_JOYBUTTONDOWN || event.type == SDL_JOYBUTTONUP)
    {
        bool pressType = event.type == SDL_JOYBUTTONDOWN ? true : false;
        /*
ps5: 0 x, 1 o, 2 sqaure, 3 triangle, 4 screenshot, 5 ps, 6 menu, 7 left joystick press, 8 right joystick press, 9 L1, 10
L2, 11 up, 12 down, 13 left, 14 right, 15 central pad, 16 mute
           */
        if (event.jbutton.button == 0)
        {
            rightPad.down = pressType;
        }
    }
    else if (event.type == SDL_JOYBUTTONUP)
    {}
    else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)
    {
        bool pressing = event.type == SDL_KEYDOWN ? true : false;
        if (event.key.keysym.sym == SDL_KeyCode::SDLK_SPACE)
        {
            keyboard.space = pressing;
        }
        else if (event.key.keysym.scancode == SDL_SCANCODE_A)
        {
            leftJoyAxis.x = pressing ? -1 : 0;
        }
        else if (event.key.keysym.scancode == SDL_SCANCODE_D)
        {
            leftJoyAxis.x = pressing ? 1 : 0;
        }
        else if (event.key.keysym.scancode == SDL_SCANCODE_W)
        {
            leftJoyAxis.y = pressing ? -1 : 0;
        }
        else if (event.key.keysym.scancode == SDL_SCANCODE_S)
        {
            leftJoyAxis.y = pressing ? 1 : 0;
        }
    }
}

float Input::JoyStickRemap(int& val)
{
    val = glm::clamp(val, -32767, 32767);
    if (std::abs(val) < JoyStickDeadZone)
    {
        return 0;
    }

    return val / 32767.0f;
}
