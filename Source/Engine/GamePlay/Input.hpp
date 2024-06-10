#pragma once
#include <SDL.h>
#include <vector>

class Input
{
public:
    // left joystick
    void GetMovement(float& x, float& y);

    // right joystick
    void GetLookAround(float& x, float& y);

    void PushEvent(SDL_Event& event);

    void Reset() {}

    static Input& GetSingleton();

private:
    Input();

    struct JoyAxis
    {
        float x;
        float y;
    } leftJoyAxis, rightJoyAxis;
    std::vector<SDL_Event> pendingEvents;
    const int JoyStickDeadZone = 5000;

    float JoyStickRemap(int& val);

    friend class Event;
};
