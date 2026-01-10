#include "Input.hpp"
#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

namespace Details
{
struct Input
{
    SDL_GameController* gameController = nullptr;

    int2 mousePosition;

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
    input.gameplayInput = enabled;
}

void Input::Reset()
{
    input.FrameReset();
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