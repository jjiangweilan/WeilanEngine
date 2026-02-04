#include "Input.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/MiddleLayer/SystemInfo.hpp"
#include <array>
#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

namespace Details
{
struct Input
{
    SDL_GameController* gameController = nullptr;

    int2 mousePosition;
    bool hasMouseUV = false;
    float2 lastMouseUV = {0.0f, 0.0f};
    float mouseWheelDelta = 0.0f;
    std::array<bool, 3> mouseButtons = {false, false, false};
    std::array<bool, 3> mouseButtonsPressed = {false, false, false};
    std::array<bool, 3> mouseButtonsReleased = {false, false, false};
    std::array<bool, SDL_NUM_SCANCODES> keyDown = {};
    std::array<bool, SDL_NUM_SCANCODES> keyPressed = {};
    std::array<bool, SDL_NUM_SCANCODES> keyReleased = {};

    struct GamepadInstance
    {
        /*
            xbox: 0 A, 1 B, 2 X, 3 Y, 4 Left Bumper, 5 Right Bumper
            dualsense: 0 Cross, 1 Circle, 2 Sqaure, 3 Triangle, 4 screenshot, 5 ps, 6 menu, 7 left joystick press, 8
                       right joystick press, 9 L1, 10 L2, 11 up, 12 down, 13 left, 14 right, 15 central pad, 16 mute
        */
        bool buttons[17] = {false};
        bool buttonPressed[17] = {false};

        float2 axis[2]{};

        /**
         * 0 left, 1 right
         */
        float triggers[2];
    };

    struct Pad
    {
        bool up;
        bool down;
        bool left;
        bool right;

    } dPad;

    struct PadPressed
    {
        bool up;
        bool down;
        bool left;
        bool right;

    } rightPadPressed;

    struct Keyboard
    {
        bool w, a, s, d;
        bool space;
    } keyboard;

    /**
     * @brief stores state of actual gamepad instances.
     * The index is matching the id of GamepadInstance.
     */
    std::vector<std::unique_ptr<GamepadInstance>> gamepads;

    const int JoyStickDeadZone = 5000;
    bool gameplayInput = false;

    /**
     * @brief should be called once per frame
     */
    void FrameReset()
    {
        for (auto& p : gamepads)
        {
            memset(p->buttonPressed, 0, sizeof(p->buttonPressed));
        }
        mouseButtonsPressed.fill(false);
        mouseButtonsReleased.fill(false);
        keyPressed.fill(false);
        keyReleased.fill(false);
        mouseWheelDelta = 0.0f;
    }

    GamepadInstance* GetGamepad(int id)
    {
        while (id >= gamepads.size())
        {
            gamepads.push_back(std::make_unique<GamepadInstance>());
        }

        return gamepads[id].get();
    }

    void PushEvent(SDL_Event& event)
    {
        if (gameplayInput)
        {
            if (event.type == SDL_JOYAXISMOTION)
            {
                if (event.jbutton.which != -1)
                {
                    auto pad = GetGamepad(event.jbutton.which);

                    if (pad != nullptr)
                    {
                        if (event.jaxis.axis == 0) // left x
                        {
                            int val = static_cast<int>(event.jaxis.value);
                            pad->axis[0].x = JoyStickRemap(val);
                        }
                        else if (event.jaxis.axis == 1) // left y
                        {
                            int val = -static_cast<int>(event.jaxis.value);
                            pad->axis[0].y = JoyStickRemap(val);
                        }
                        else if (event.jaxis.axis == 2) // right x
                        {
                            int val = static_cast<int>(event.jaxis.value);
                            pad->axis[1].x = JoyStickRemap(val);
                        }
                        else if (event.jaxis.axis == 3) // right y
                        {
                            int val = -static_cast<int>(event.jaxis.value);
                            pad->axis[1].y = JoyStickRemap(val);
                        }
                        else if (event.jaxis.axis == 4) // left trigger
                        {
                            int val = static_cast<int>(event.jaxis.value);
                            pad->triggers[0] = TriggerRemap(val);
                        }
                        else if (event.jaxis.axis == 5) // left trigger
                        {
                            int val = static_cast<int>(event.jaxis.value);
                            pad->triggers[1] = TriggerRemap(val);
                        }
                    }
                }
            }
            else if (event.type == SDL_JOYBUTTONDOWN || event.type == SDL_JOYBUTTONUP)
            {
                bool pressType = event.type == SDL_JOYBUTTONDOWN ? true : false;
                if (event.jbutton.which != -1)
                {
                    auto pad = GetGamepad(event.jbutton.which);
                    if (pad != nullptr)
                    {
                        uint8_t buttonIdx = event.jbutton.button;

                        if (buttonIdx < 17)
                        {
                            pad->buttons[buttonIdx] = pressType;
                            if (pressType)
                            {
                                pad->buttonPressed[buttonIdx] = true;
                            }
                        }
                    }
                }

                // deprecating
                if (event.jbutton.button == 2)
                {
                    rightPadPressed.left = pressType;
                }
            }
            else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)
            {
                bool pressing = event.type == SDL_KEYDOWN ? true : false;
                auto pad = GetGamepad(0);
                auto scancode = event.key.keysym.scancode;
                if (scancode >= 0 && scancode < SDL_NUM_SCANCODES)
                {
                    keyDown[scancode] = pressing;
                    if (pressing && event.key.repeat == 0)
                    {
                        keyPressed[scancode] = true;
                    }
                    if (!pressing)
                    {
                        keyReleased[scancode] = true;
                    }
                }
                if (event.key.keysym.sym == SDL_KeyCode::SDLK_SPACE)
                {
                    keyboard.space = pressing;
                }
                else if (event.key.keysym.scancode == SDL_SCANCODE_A)
                {
                    pad->axis[0].x = pressing ? -1 : 0;
                }
                else if (event.key.keysym.scancode == SDL_SCANCODE_D)
                {
                    pad->axis[0].x = pressing ? 1 : 0;
                }
                else if (event.key.keysym.scancode == SDL_SCANCODE_W)
                {
                    pad->axis[0].y = pressing ? 1 : 0;
                }
                else if (event.key.keysym.scancode == SDL_SCANCODE_S)
                {
                    pad->axis[0].y = pressing ? -1 : 0;
                }
            }
            else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP)
            {
                bool pressing = event.type == SDL_MOUSEBUTTONDOWN;
                int index = -1;
                if (event.button.button == SDL_BUTTON_LEFT)
                    index = 0;
                else if (event.button.button == SDL_BUTTON_RIGHT)
                    index = 1;
                else if (event.button.button == SDL_BUTTON_MIDDLE)
                    index = 2;

                if (index >= 0)
                {
                    mouseButtons[index] = pressing;
                    if (pressing)
                    {
                        mouseButtonsPressed[index] = true;
                    }
                    else
                    {
                        mouseButtonsReleased[index] = true;
                    }
                }
            }
            else if (event.type == SDL_MOUSEWHEEL)
            {
                mouseWheelDelta += static_cast<float>(event.wheel.y);
            }
        }

        if (event.type == SDL_CONTROLLERDEVICEADDED)
        {
            if (SDL_IsGameController(event.cdevice.which) && gameController == nullptr)
            {
                gameController = SDL_GameControllerOpen(event.cdevice.which);
                if (gameController)
                {
                    spdlog::info("Controller connected: {}\n", SDL_GameControllerName(gameController));
                }
            }
            auto pad = GetGamepad(event.jbutton.which);
        }
        else if (event.type == SDL_CONTROLLERDEVICEREMOVED)
        {
            if (gameController != nullptr)
            {
                spdlog::info("Controller disconnected {}!\n", SDL_GameControllerName(gameController));
                SDL_GameControllerClose(gameController);
            }
        }
    }

private:
    float JoyStickRemap(int& val)
    {
        val = glm::clamp(val, -32767, 32767);
        if (std::abs(val) < JoyStickDeadZone)
        {
            return 0;
        }

        return val / 32767.0f;
    }

    float TriggerRemap(int& val)
    {
        val = glm::clamp(val, -32767, 32767);

        return glm::clamp(val / 32767.0f * 0.5f + 0.5f, 0.0f, 1.0f);
    }
};
}; // namespace Details
   //

static Details::Input input;

void Input::PushEvent(SDL_Event& event)
{
    input.PushEvent(event);
}

void Input::UpdateState()
{
    SDL_GetMouseState(&input.mousePosition.x, &input.mousePosition.y);
}

bool Input::IsKeyDown(InputScancode key)
{
    if (!input.gameplayInput)
    {
        return false;
    }

    int scancode = static_cast<int>(key);
    if (scancode < 0 || scancode >= SDL_NUM_SCANCODES)
    {
        return false;
    }
    return input.keyDown[scancode];
}

bool Input::IsKeyPressed(InputScancode key)
{
    if (!input.gameplayInput)
    {
        return false;
    }

    int scancode = static_cast<int>(key);
    if (scancode < 0 || scancode >= SDL_NUM_SCANCODES)
    {
        return false;
    }
    return input.keyPressed[scancode];
}

bool Input::IsKeyReleased(InputScancode key)
{
    if (!input.gameplayInput)
    {
        return false;
    }

    int scancode = static_cast<int>(key);
    if (scancode < 0 || scancode >= SDL_NUM_SCANCODES)
    {
        return false;
    }
    return input.keyReleased[scancode];
}

float Input::GetMovementX()
{
    float x = input.GetGamepad(0)->axis[0].x;
    return x;
}

float Input::GetMovementY()
{
    float y = input.GetGamepad(0)->axis[0].y;
    return y;
}

bool Input::IsInteractPressed()
{
    return input.rightPadPressed.left;
}

void Input::GetMovement(float& x, float& y)
{
    x = GetMovementX();
    y = GetMovementY();
}

int2 Input::GetMousePosition()
{
    return input.mousePosition;
}

float2 Input::GetMouseUV()
{
    auto& systemInfo = SystemInfo::Singleton();
    int2 origin = systemInfo.GetGameViewOrigin();
    float2 screenSize = systemInfo.GetScreenSize();

    if (screenSize.x <= 0.0f || screenSize.y <= 0.0f)
    {
        return {0.0f, 0.0f};
    }

    float2 mouseInView = {
        static_cast<float>(input.mousePosition.x - origin.x),
        static_cast<float>(input.mousePosition.y - origin.y)
    };
    return {mouseInView.x / screenSize.x, mouseInView.y / screenSize.y};
}

float Input::GetMouseWheelDelta()
{
    if (!input.gameplayInput)
    {
        return 0.0f;
    }
    return input.mouseWheelDelta;
}

void Input::GetLookAround(float& x, float& y)
{
    x = input.GetGamepad(0)->axis[1].x;
    y = input.GetGamepad(0)->axis[1].y;
}

float Input::GetLookAroundX()
{
    return input.GetGamepad(0)->axis[1].x;
}

float Input::GetLookAroundY()
{
    return input.GetGamepad(0)->axis[1].y;
}

bool Input::Jump()
{
    return input.keyboard.space || input.GetGamepad(0)->buttons[0];
}

void Input::SetGameplayInput(bool enabled)
{
    if (input.gameplayInput == enabled)
    {
        return;
    }
    input.gameplayInput = enabled;
    input.keyDown.fill(false);
    input.keyPressed.fill(false);
    input.keyReleased.fill(false);
    input.mouseButtons.fill(false);
    input.mouseButtonsPressed.fill(false);
    input.mouseButtonsReleased.fill(false);
    input.mouseWheelDelta = 0.0f;
    input.hasMouseUV = false;
}

void Input::Reset()
{
    input.FrameReset();
}

bool Input::IsMouseButtonDown(MouseButton mouseButton)
{
    if (!input.gameplayInput)
    {
        return false;
    }
    switch (mouseButton)
    {
        case MouseButton::Left:
            return input.mouseButtons[0];
        case MouseButton::Right:
            return input.mouseButtons[1];
        case MouseButton::Middle:
            return input.mouseButtons[2];
        default:
            return false;
    }
}

bool Input::IsMouseButtonPressed(MouseButton mouseButton)
{
    if (!input.gameplayInput)
    {
        return false;
    }

    switch (mouseButton)
    {
        case MouseButton::Left:
            return input.mouseButtonsPressed[0];
        case MouseButton::Right:
            return input.mouseButtonsPressed[1];
        case MouseButton::Middle:
            return input.mouseButtonsPressed[2];
        default:
            return false;
    }
}

bool Input::IsMouseButtonReleased(MouseButton mouseButton)
{
    if (!input.gameplayInput)
    {
        return false;
    }

    switch (mouseButton)
    {
        case MouseButton::Left:
            return input.mouseButtonsReleased[0];
        case MouseButton::Right:
            return input.mouseButtonsReleased[1];
        case MouseButton::Middle:
            return input.mouseButtonsReleased[2];
        default:
            return false;
    }
}

bool Input::HasFocus()
{
    auto driver = GetGfxDriver();
    if (!driver)
    {
        return false;
    }
    SDL_Window* window = driver->GetSDLWindow();
    if (!window)
    {
        return false;
    }
    return (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) != 0;
}

float2 Input::GetMouseDelta()
{
    auto& systemInfo = SystemInfo::Singleton();
    int2 origin = systemInfo.GetGameViewOrigin();
    float2 screenSize = systemInfo.GetScreenSize();

    if (screenSize.x <= 0.0f || screenSize.y <= 0.0f)
    {
        input.hasMouseUV = false;
        return {0.0f, 0.0f};
    }

    float2 mouseInView = {
        static_cast<float>(input.mousePosition.x - origin.x),
        static_cast<float>(input.mousePosition.y - origin.y)
    };
    float2 mouseUV = {mouseInView.x / screenSize.x, mouseInView.y / screenSize.y};

    if (!input.hasMouseUV)
    {
        input.lastMouseUV = mouseUV;
        input.hasMouseUV = true;
        return {0.0f, 0.0f};
    }

    float2 delta = {mouseUV.x - input.lastMouseUV.x, mouseUV.y - input.lastMouseUV.y};
    input.lastMouseUV = mouseUV;
    return delta;
}

Gamepad Input::GetGamepad(int padIdx)
{
    return Gamepad(padIdx);
}

bool Gamepad::IsButtonPressed(uint8_t idx)
{
    if (idx < 5)
    {
        return input.GetGamepad(gamepadID)->buttonPressed[ButtonRemap(idx)];
    }

    return false;
}

bool Gamepad::IsBumperPressed(uint8_t idx)
{
    if (idx < 2)
    {
        return input.GetGamepad(gamepadID)->buttonPressed[ButtonRemap(idx + 4)];
    }
    return false;
}

float Gamepad::GetTrigger(uint8_t idx)
{
    if (idx < 2)
    {
        return input.GetGamepad(gamepadID)->triggers[idx];
    }
    return 0;
}

float2 Gamepad::GetAxis(uint8_t idx)
{
    if (idx < 2)
    {
        return input.GetGamepad(gamepadID)->axis[idx];
    }

    return {0, 0};
}
