#pragma once
#include "Libs/Math.hpp"
#include <SDL.h>

struct Gamepad
{
    Gamepad(int idx) : gamepadID(idx) {}
    /**
     * reserved for multi gamepad support,
     * matching the ID of SDL_JoystickInstanceID
     */
    int gamepadID = 0;

    /**
     * 0: xbox controller, 1: dualsense
     */
    int gamepadType;

    /**
     * @brief: check if a button is pressed for the current frame, button index is basing on the return index of xbox
     * controller from SDL
     *
     * @param idx: 0 'X', 1 'Y', 2 'A', 3 'B'.
     */
    bool IsButtonPressed(uint8_t idx);

    /**
     * @brief: check if a bumper is pressed for the current frame
     *
     * @param idx 0 'left bumper', 1 'right bumper'
     */
    bool IsBumperPressed(uint8_t idx);

    /**
     * @brief: get trigger state for the current frame
     *
     * @return: 0.0f - 1.0f
     */
    float GetTrigger(uint8_t idx);

    /**
     * @brief; get axis state
     *
     * @param idx 0 'left axis', 1 'right axis'
     */
    float2 GetAxis(uint8_t idx);

private:
    /**
     * @brief: remap the button index based on controller type(xbox, dualsense, switch, etc...)
     */
    // TODO: implemented this as an int to int array remap
    uint8_t ButtonRemap(uint8_t idx) { return idx; }
};

class Input
{
public:
    static Gamepad GetGamepad(int padIdx = 0);
    static float GetMovementX();
    static float GetMovementY();
    static bool IsInteractPressed();
    static void GetMovement(float& x, float& y);
    static void GetLookAround(float& x, float& y);
    static float GetLookAroundX();
    static float GetLookAroundY();
    static bool Jump();
    static void PushEvent(SDL_Event& event);
    static void SetGameplayInput(bool enabled);
    static void Reset();
};
