#include "Input.hpp"
#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

Input::Input() {}
Input& Input::GetSingleton()
{
    static Input input;
    return input;
}

void Input::GetMovement(float& x, float& y)
{
    x = leftJoyAxis.x;
    y = leftJoyAxis.y;
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
