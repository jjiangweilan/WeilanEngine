#pragma once
#include <SDL.h>
#include <vector>

class Input
{
public:
    // left joystick
    inline void GetMovement(float& x, float& y)
    {
        x = leftJoyAxis.x;
        y = leftJoyAxis.y;
    }

    // right joystick
    inline void GetLookAround(float& x, float& y)
    {
        x = rightJoyAxis.x;
        y = rightJoyAxis.y;
    }

    // ps5: is x button down
    inline bool Jump()
    {
        return rightPad.down;
    }

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

    struct Pad
    {
        bool up;
        bool down;
        bool left;
        bool right;

    } dPad, rightPad;
    std::vector<SDL_Event> pendingEvents;
    const int JoyStickDeadZone = 5000;

    float JoyStickRemap(int& val);

    friend class Event;
};
