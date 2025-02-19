#pragma once
#include <SDL.h>
#include <vector>

class Input
{
public:
    static float GetMovementX()
    {
        float x, y;
        GetSingleton().GetMovementImpl(x, y);
        return x;
    }

    static float GetMovementY()
    {
        float x, y;
        GetSingleton().GetMovementImpl(x, y);
        return y;
    }

    static void GetMovement(float& x, float& y) { GetSingleton().GetMovementImpl(x, y); }

    static void GetLookAround(float& x, float& y) { GetSingleton().GetLookAroundImpl(x, y); }

    static float GetLookAroundX()
    {
        float x, y;
        GetSingleton().GetLookAroundImpl(x, y);
        return x;
    }

    static float GetLookAroundY()
    {
        float x, y;
        GetSingleton().GetLookAroundImpl(x, y);
        return y;
    }

    static bool Jump() { return GetSingleton().JumpImpl(); }

    void PushEvent(SDL_Event& event);

    void SetGameplayInput(bool enabled) { this->gameplayInput = enabled; }
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

    struct Keyboard
    {
        bool w, a, s, d;
        bool space;
    } keyboard;
    std::vector<SDL_Event> pendingEvents;
    const int JoyStickDeadZone = 5000;
    bool gameplayInput = false;

    float JoyStickRemap(int& val);

    // left joystick
    inline void GetMovementImpl(float& x, float& y)
    {
        x = leftJoyAxis.x;
        y = leftJoyAxis.y;
    }

    // right joystick
    inline void GetLookAroundImpl(float& x, float& y)
    {
        x = rightJoyAxis.x;
        y = rightJoyAxis.y;
    }

    // ps5: is x button down
    inline bool JumpImpl() { return rightPad.down || keyboard.space; }

    friend class Event;
};
