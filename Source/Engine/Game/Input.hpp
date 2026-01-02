#pragma once
#include "Engine/Library/DynamicArray.hpp"
#include "Engine/Library/Math.hpp"
#include <SDL.h>

struct [[LuaClass]] Gamepad
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
    [[LuaFn]] bool IsButtonPressed(uint8_t idx);

    /**
     * @brief: check if a bumper is pressed for the current frame
     *
     * @param idx 0 'left bumper', 1 'right bumper'
     */
    [[LuaFn]] bool IsBumperPressed(uint8_t idx);

    /**
     * @brief: get trigger state for the current frame
     *
     * @return: 0.0f - 1.0f
     */
    [[LuaFn]] float GetTrigger(uint8_t idx);

    /**
     * @brief; get axis state
     *
     * @param idx 0 'left axis', 1 'right axis'
     */
    [[LuaFn]] float2 GetAxis(uint8_t idx);

private:
    /**
     * @brief: remap the button index based on controller type(xbox, dualsense, switch, etc...)
     */
    // TODO: implemented this as an int to int array remap
    uint8_t ButtonRemap(uint8_t idx) { return idx; }
};

/**
 * @class Input
 * @brief We need an improvement on Input handling, like jump command should append to a process queue instead of
 * directly query the button state. An input command queue is very command specific, so just specialize it
 *
 */

class [[LuaClass]] Input
{
public:
    [[LuaFn]] static Gamepad GetGamepad(int padIdx = 0);
    static int2 GetMousePosition();
    [[LuaFn]] static float GetMovementX();
    [[LuaFn]] static float GetMovementY();
    [[LuaFn]] static bool IsInteractPressed();
    static void GetMovement(float& x, float& y);
    static void GetLookAround(float& x, float& y);
    [[LuaFn]] static float GetLookAroundX();
    [[LuaFn]] static float GetLookAroundY();
    [[LuaFn]] static bool Jump();
    static void PushEvent(SDL_Event& event);
    static void SetGameplayInput(bool enabled);
    static void Reset();
    static void UpdateState();
};
