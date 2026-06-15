// GENERATED FILE - DO NOT EDIT
#include "Engine/Runtime/System/ScriptingBackend/LuaBindings.hpp"
#include "Engine/Runtime/System/ScriptingBackend/LuaBindings_Private.hpp"

#include "Engine/Game/Input.hpp"
#include "Engine/MiddleLayer/EngineDebug.hpp"
#include "Engine/Runtime/Physics.hpp"
#include "Engine/Runtime/Object/Component/AnimationPlayer.hpp"
#include "Engine/Runtime/Object/Component/Boids.hpp"
#include "Engine/Runtime/Object/Component/PhysicsBody.hpp"
#include "Engine/Runtime/Object/GameObject/GameObject.hpp"
#include "Engine/Runtime/Object/GameObject/Prefab.hpp"
#include "Engine/Runtime/System/SceneManager/Scene.hpp"

void BindGeneratedClasses(lua_State* L)
{
    LuaBinder<Gamepad> binder_Gamepad(L);
    binder_Gamepad.Begin("Gamepad")
        .BindMemFn("IsButtonPressed", &Gamepad::IsButtonPressed) // bool(uint8_t idx)
        .BindMemFn("IsBumperPressed", &Gamepad::IsBumperPressed) // bool(uint8_t idx)
        .BindMemFn("GetTrigger", &Gamepad::GetTrigger) // float(uint8_t idx)
        .BindMemFn("GetAxis", &Gamepad::GetAxis) // float2(uint8_t idx)
        .End();

    LuaBinder<Input> binder_Input(L);
    binder_Input.Begin("Input")
        .BindStaticFn("GetGamepad", &Input::GetGamepad) // Gamepad()
        .BindStaticFn("GetMovementX", &Input::GetMovementX) // float()
        .BindStaticFn("GetMovementY", &Input::GetMovementY) // float()
        .BindStaticFn("IsInteractPressed", &Input::IsInteractPressed) // bool()
        .BindStaticFn("GetLookAroundX", &Input::GetLookAroundX) // float()
        .BindStaticFn("GetLookAroundY", &Input::GetLookAroundY) // float()
        .BindStaticFn("Jump", &Input::Jump) // bool()
        .End();

    // Bind Enum GamepadButtons
    lua_newtable(L);
    lua_pushinteger(L, static_cast<int>(GamepadButtons::X));
    lua_setfield(L, -2, "X");
    lua_pushinteger(L, static_cast<int>(GamepadButtons::Y));
    lua_setfield(L, -2, "Y");
    lua_pushinteger(L, static_cast<int>(GamepadButtons::A));
    lua_setfield(L, -2, "A");
    lua_pushinteger(L, static_cast<int>(GamepadButtons::B));
    lua_setfield(L, -2, "B");
    lua_setfield(L, -2, "GamepadButtons");

    // Bind Enum InputScancode
    lua_newtable(L);
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_UNKNOWN));
    lua_setfield(L, -2, "Key_UNKNOWN");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_A));
    lua_setfield(L, -2, "Key_A");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_B));
    lua_setfield(L, -2, "Key_B");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_C));
    lua_setfield(L, -2, "Key_C");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_D));
    lua_setfield(L, -2, "Key_D");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_E));
    lua_setfield(L, -2, "Key_E");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_F));
    lua_setfield(L, -2, "Key_F");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_G));
    lua_setfield(L, -2, "Key_G");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_H));
    lua_setfield(L, -2, "Key_H");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_I));
    lua_setfield(L, -2, "Key_I");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_J));
    lua_setfield(L, -2, "Key_J");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_K));
    lua_setfield(L, -2, "Key_K");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_L));
    lua_setfield(L, -2, "Key_L");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_M));
    lua_setfield(L, -2, "Key_M");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_N));
    lua_setfield(L, -2, "Key_N");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_O));
    lua_setfield(L, -2, "Key_O");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_P));
    lua_setfield(L, -2, "Key_P");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_Q));
    lua_setfield(L, -2, "Key_Q");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_R));
    lua_setfield(L, -2, "Key_R");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_S));
    lua_setfield(L, -2, "Key_S");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_T));
    lua_setfield(L, -2, "Key_T");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_U));
    lua_setfield(L, -2, "Key_U");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_V));
    lua_setfield(L, -2, "Key_V");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_W));
    lua_setfield(L, -2, "Key_W");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_X));
    lua_setfield(L, -2, "Key_X");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_Y));
    lua_setfield(L, -2, "Key_Y");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_Z));
    lua_setfield(L, -2, "Key_Z");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_1));
    lua_setfield(L, -2, "Key_1");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_2));
    lua_setfield(L, -2, "Key_2");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_3));
    lua_setfield(L, -2, "Key_3");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_4));
    lua_setfield(L, -2, "Key_4");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_5));
    lua_setfield(L, -2, "Key_5");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_6));
    lua_setfield(L, -2, "Key_6");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_7));
    lua_setfield(L, -2, "Key_7");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_8));
    lua_setfield(L, -2, "Key_8");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_9));
    lua_setfield(L, -2, "Key_9");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_0));
    lua_setfield(L, -2, "Key_0");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_RETURN));
    lua_setfield(L, -2, "Key_RETURN");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_ESCAPE));
    lua_setfield(L, -2, "Key_ESCAPE");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_BACKSPACE));
    lua_setfield(L, -2, "Key_BACKSPACE");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_TAB));
    lua_setfield(L, -2, "Key_TAB");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_SPACE));
    lua_setfield(L, -2, "Key_SPACE");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_MINUS));
    lua_setfield(L, -2, "Key_MINUS");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_EQUALS));
    lua_setfield(L, -2, "Key_EQUALS");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_LEFTBRACKET));
    lua_setfield(L, -2, "Key_LEFTBRACKET");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_RIGHTBRACKET));
    lua_setfield(L, -2, "Key_RIGHTBRACKET");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_BACKSLASH));
    lua_setfield(L, -2, "Key_BACKSLASH");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_NONUSHASH));
    lua_setfield(L, -2, "Key_NONUSHASH");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_SEMICOLON));
    lua_setfield(L, -2, "Key_SEMICOLON");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_APOSTROPHE));
    lua_setfield(L, -2, "Key_APOSTROPHE");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_GRAVE));
    lua_setfield(L, -2, "Key_GRAVE");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_COMMA));
    lua_setfield(L, -2, "Key_COMMA");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_PERIOD));
    lua_setfield(L, -2, "Key_PERIOD");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_SLASH));
    lua_setfield(L, -2, "Key_SLASH");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_CAPSLOCK));
    lua_setfield(L, -2, "Key_CAPSLOCK");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_F1));
    lua_setfield(L, -2, "Key_F1");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_F2));
    lua_setfield(L, -2, "Key_F2");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_F3));
    lua_setfield(L, -2, "Key_F3");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_F4));
    lua_setfield(L, -2, "Key_F4");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_F5));
    lua_setfield(L, -2, "Key_F5");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_F6));
    lua_setfield(L, -2, "Key_F6");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_F7));
    lua_setfield(L, -2, "Key_F7");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_F8));
    lua_setfield(L, -2, "Key_F8");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_F9));
    lua_setfield(L, -2, "Key_F9");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_F10));
    lua_setfield(L, -2, "Key_F10");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_F11));
    lua_setfield(L, -2, "Key_F11");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_F12));
    lua_setfield(L, -2, "Key_F12");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_PRINTSCREEN));
    lua_setfield(L, -2, "Key_PRINTSCREEN");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_SCROLLLOCK));
    lua_setfield(L, -2, "Key_SCROLLLOCK");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_PAUSE));
    lua_setfield(L, -2, "Key_PAUSE");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_INSERT));
    lua_setfield(L, -2, "Key_INSERT");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_HOME));
    lua_setfield(L, -2, "Key_HOME");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_PAGEUP));
    lua_setfield(L, -2, "Key_PAGEUP");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_DELETE));
    lua_setfield(L, -2, "Key_DELETE");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_END));
    lua_setfield(L, -2, "Key_END");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_PAGEDOWN));
    lua_setfield(L, -2, "Key_PAGEDOWN");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_RIGHT));
    lua_setfield(L, -2, "Key_RIGHT");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_LEFT));
    lua_setfield(L, -2, "Key_LEFT");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_DOWN));
    lua_setfield(L, -2, "Key_DOWN");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_UP));
    lua_setfield(L, -2, "Key_UP");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_NUMLOCKCLEAR));
    lua_setfield(L, -2, "Key_NUMLOCKCLEAR");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_DIVIDE));
    lua_setfield(L, -2, "Key_KP_DIVIDE");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_MULTIPLY));
    lua_setfield(L, -2, "Key_KP_MULTIPLY");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_MINUS));
    lua_setfield(L, -2, "Key_KP_MINUS");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_PLUS));
    lua_setfield(L, -2, "Key_KP_PLUS");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_ENTER));
    lua_setfield(L, -2, "Key_KP_ENTER");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_1));
    lua_setfield(L, -2, "Key_KP_1");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_2));
    lua_setfield(L, -2, "Key_KP_2");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_3));
    lua_setfield(L, -2, "Key_KP_3");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_4));
    lua_setfield(L, -2, "Key_KP_4");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_5));
    lua_setfield(L, -2, "Key_KP_5");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_6));
    lua_setfield(L, -2, "Key_KP_6");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_7));
    lua_setfield(L, -2, "Key_KP_7");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_8));
    lua_setfield(L, -2, "Key_KP_8");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_9));
    lua_setfield(L, -2, "Key_KP_9");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_0));
    lua_setfield(L, -2, "Key_KP_0");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_PERIOD));
    lua_setfield(L, -2, "Key_KP_PERIOD");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_NONUSBACKSLASH));
    lua_setfield(L, -2, "Key_NONUSBACKSLASH");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_APPLICATION));
    lua_setfield(L, -2, "Key_APPLICATION");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_POWER));
    lua_setfield(L, -2, "Key_POWER");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_EQUALS));
    lua_setfield(L, -2, "Key_KP_EQUALS");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_F13));
    lua_setfield(L, -2, "Key_F13");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_F14));
    lua_setfield(L, -2, "Key_F14");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_F15));
    lua_setfield(L, -2, "Key_F15");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_F16));
    lua_setfield(L, -2, "Key_F16");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_F17));
    lua_setfield(L, -2, "Key_F17");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_F18));
    lua_setfield(L, -2, "Key_F18");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_F19));
    lua_setfield(L, -2, "Key_F19");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_F20));
    lua_setfield(L, -2, "Key_F20");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_F21));
    lua_setfield(L, -2, "Key_F21");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_F22));
    lua_setfield(L, -2, "Key_F22");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_F23));
    lua_setfield(L, -2, "Key_F23");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_F24));
    lua_setfield(L, -2, "Key_F24");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_EXECUTE));
    lua_setfield(L, -2, "Key_EXECUTE");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_HELP));
    lua_setfield(L, -2, "Key_HELP");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_MENU));
    lua_setfield(L, -2, "Key_MENU");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_SELECT));
    lua_setfield(L, -2, "Key_SELECT");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_STOP));
    lua_setfield(L, -2, "Key_STOP");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_AGAIN));
    lua_setfield(L, -2, "Key_AGAIN");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_UNDO));
    lua_setfield(L, -2, "Key_UNDO");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_CUT));
    lua_setfield(L, -2, "Key_CUT");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_COPY));
    lua_setfield(L, -2, "Key_COPY");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_PASTE));
    lua_setfield(L, -2, "Key_PASTE");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_FIND));
    lua_setfield(L, -2, "Key_FIND");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_MUTE));
    lua_setfield(L, -2, "Key_MUTE");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_VOLUMEUP));
    lua_setfield(L, -2, "Key_VOLUMEUP");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_VOLUMEDOWN));
    lua_setfield(L, -2, "Key_VOLUMEDOWN");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_COMMA));
    lua_setfield(L, -2, "Key_KP_COMMA");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_EQUALSAS400));
    lua_setfield(L, -2, "Key_KP_EQUALSAS400");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_INTERNATIONAL1));
    lua_setfield(L, -2, "Key_INTERNATIONAL1");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_INTERNATIONAL2));
    lua_setfield(L, -2, "Key_INTERNATIONAL2");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_INTERNATIONAL3));
    lua_setfield(L, -2, "Key_INTERNATIONAL3");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_INTERNATIONAL4));
    lua_setfield(L, -2, "Key_INTERNATIONAL4");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_INTERNATIONAL5));
    lua_setfield(L, -2, "Key_INTERNATIONAL5");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_INTERNATIONAL6));
    lua_setfield(L, -2, "Key_INTERNATIONAL6");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_INTERNATIONAL7));
    lua_setfield(L, -2, "Key_INTERNATIONAL7");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_INTERNATIONAL8));
    lua_setfield(L, -2, "Key_INTERNATIONAL8");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_INTERNATIONAL9));
    lua_setfield(L, -2, "Key_INTERNATIONAL9");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_LANG1));
    lua_setfield(L, -2, "Key_LANG1");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_LANG2));
    lua_setfield(L, -2, "Key_LANG2");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_LANG3));
    lua_setfield(L, -2, "Key_LANG3");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_LANG4));
    lua_setfield(L, -2, "Key_LANG4");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_LANG5));
    lua_setfield(L, -2, "Key_LANG5");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_LANG6));
    lua_setfield(L, -2, "Key_LANG6");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_LANG7));
    lua_setfield(L, -2, "Key_LANG7");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_LANG8));
    lua_setfield(L, -2, "Key_LANG8");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_LANG9));
    lua_setfield(L, -2, "Key_LANG9");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_ALTERASE));
    lua_setfield(L, -2, "Key_ALTERASE");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_SYSREQ));
    lua_setfield(L, -2, "Key_SYSREQ");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_CANCEL));
    lua_setfield(L, -2, "Key_CANCEL");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_CLEAR));
    lua_setfield(L, -2, "Key_CLEAR");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_PRIOR));
    lua_setfield(L, -2, "Key_PRIOR");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_RETURN2));
    lua_setfield(L, -2, "Key_RETURN2");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_SEPARATOR));
    lua_setfield(L, -2, "Key_SEPARATOR");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_OUT));
    lua_setfield(L, -2, "Key_OUT");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_OPER));
    lua_setfield(L, -2, "Key_OPER");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_CLEARAGAIN));
    lua_setfield(L, -2, "Key_CLEARAGAIN");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_CRSEL));
    lua_setfield(L, -2, "Key_CRSEL");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_EXSEL));
    lua_setfield(L, -2, "Key_EXSEL");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_00));
    lua_setfield(L, -2, "Key_KP_00");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_000));
    lua_setfield(L, -2, "Key_KP_000");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_THOUSANDSSEPARATOR));
    lua_setfield(L, -2, "Key_THOUSANDSSEPARATOR");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_DECIMALSEPARATOR));
    lua_setfield(L, -2, "Key_DECIMALSEPARATOR");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_CURRENCYUNIT));
    lua_setfield(L, -2, "Key_CURRENCYUNIT");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_CURRENCYSUBUNIT));
    lua_setfield(L, -2, "Key_CURRENCYSUBUNIT");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_LEFTPAREN));
    lua_setfield(L, -2, "Key_KP_LEFTPAREN");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_RIGHTPAREN));
    lua_setfield(L, -2, "Key_KP_RIGHTPAREN");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_LEFTBRACE));
    lua_setfield(L, -2, "Key_KP_LEFTBRACE");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_RIGHTBRACE));
    lua_setfield(L, -2, "Key_KP_RIGHTBRACE");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_TAB));
    lua_setfield(L, -2, "Key_KP_TAB");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_BACKSPACE));
    lua_setfield(L, -2, "Key_KP_BACKSPACE");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_A));
    lua_setfield(L, -2, "Key_KP_A");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_B));
    lua_setfield(L, -2, "Key_KP_B");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_C));
    lua_setfield(L, -2, "Key_KP_C");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_D));
    lua_setfield(L, -2, "Key_KP_D");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_E));
    lua_setfield(L, -2, "Key_KP_E");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_F));
    lua_setfield(L, -2, "Key_KP_F");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_XOR));
    lua_setfield(L, -2, "Key_KP_XOR");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_POWER));
    lua_setfield(L, -2, "Key_KP_POWER");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_PERCENT));
    lua_setfield(L, -2, "Key_KP_PERCENT");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_LESS));
    lua_setfield(L, -2, "Key_KP_LESS");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_GREATER));
    lua_setfield(L, -2, "Key_KP_GREATER");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_AMPERSAND));
    lua_setfield(L, -2, "Key_KP_AMPERSAND");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_DBLAMPERSAND));
    lua_setfield(L, -2, "Key_KP_DBLAMPERSAND");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_VERTICALBAR));
    lua_setfield(L, -2, "Key_KP_VERTICALBAR");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_DBLVERTICALBAR));
    lua_setfield(L, -2, "Key_KP_DBLVERTICALBAR");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_COLON));
    lua_setfield(L, -2, "Key_KP_COLON");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_HASH));
    lua_setfield(L, -2, "Key_KP_HASH");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_SPACE));
    lua_setfield(L, -2, "Key_KP_SPACE");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_AT));
    lua_setfield(L, -2, "Key_KP_AT");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_EXCLAM));
    lua_setfield(L, -2, "Key_KP_EXCLAM");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_MEMSTORE));
    lua_setfield(L, -2, "Key_KP_MEMSTORE");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_MEMRECALL));
    lua_setfield(L, -2, "Key_KP_MEMRECALL");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_MEMCLEAR));
    lua_setfield(L, -2, "Key_KP_MEMCLEAR");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_MEMADD));
    lua_setfield(L, -2, "Key_KP_MEMADD");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_MEMSUBTRACT));
    lua_setfield(L, -2, "Key_KP_MEMSUBTRACT");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_MEMMULTIPLY));
    lua_setfield(L, -2, "Key_KP_MEMMULTIPLY");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_MEMDIVIDE));
    lua_setfield(L, -2, "Key_KP_MEMDIVIDE");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_PLUSMINUS));
    lua_setfield(L, -2, "Key_KP_PLUSMINUS");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_CLEAR));
    lua_setfield(L, -2, "Key_KP_CLEAR");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_CLEARENTRY));
    lua_setfield(L, -2, "Key_KP_CLEARENTRY");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_BINARY));
    lua_setfield(L, -2, "Key_KP_BINARY");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_OCTAL));
    lua_setfield(L, -2, "Key_KP_OCTAL");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_DECIMAL));
    lua_setfield(L, -2, "Key_KP_DECIMAL");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KP_HEXADECIMAL));
    lua_setfield(L, -2, "Key_KP_HEXADECIMAL");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_LCTRL));
    lua_setfield(L, -2, "Key_LCTRL");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_LSHIFT));
    lua_setfield(L, -2, "Key_LSHIFT");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_LALT));
    lua_setfield(L, -2, "Key_LALT");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_LGUI));
    lua_setfield(L, -2, "Key_LGUI");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_RCTRL));
    lua_setfield(L, -2, "Key_RCTRL");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_RSHIFT));
    lua_setfield(L, -2, "Key_RSHIFT");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_RALT));
    lua_setfield(L, -2, "Key_RALT");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_RGUI));
    lua_setfield(L, -2, "Key_RGUI");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_MODE));
    lua_setfield(L, -2, "Key_MODE");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_AUDIONEXT));
    lua_setfield(L, -2, "Key_AUDIONEXT");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_AUDIOPREV));
    lua_setfield(L, -2, "Key_AUDIOPREV");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_AUDIOSTOP));
    lua_setfield(L, -2, "Key_AUDIOSTOP");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_AUDIOPLAY));
    lua_setfield(L, -2, "Key_AUDIOPLAY");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_AUDIOMUTE));
    lua_setfield(L, -2, "Key_AUDIOMUTE");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_MEDIASELECT));
    lua_setfield(L, -2, "Key_MEDIASELECT");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_WWW));
    lua_setfield(L, -2, "Key_WWW");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_MAIL));
    lua_setfield(L, -2, "Key_MAIL");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_CALCULATOR));
    lua_setfield(L, -2, "Key_CALCULATOR");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_COMPUTER));
    lua_setfield(L, -2, "Key_COMPUTER");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_AC_SEARCH));
    lua_setfield(L, -2, "Key_AC_SEARCH");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_AC_HOME));
    lua_setfield(L, -2, "Key_AC_HOME");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_AC_BACK));
    lua_setfield(L, -2, "Key_AC_BACK");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_AC_FORWARD));
    lua_setfield(L, -2, "Key_AC_FORWARD");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_AC_STOP));
    lua_setfield(L, -2, "Key_AC_STOP");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_AC_REFRESH));
    lua_setfield(L, -2, "Key_AC_REFRESH");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_AC_BOOKMARKS));
    lua_setfield(L, -2, "Key_AC_BOOKMARKS");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_BRIGHTNESSDOWN));
    lua_setfield(L, -2, "Key_BRIGHTNESSDOWN");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_BRIGHTNESSUP));
    lua_setfield(L, -2, "Key_BRIGHTNESSUP");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_DISPLAYSWITCH));
    lua_setfield(L, -2, "Key_DISPLAYSWITCH");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KBDILLUMTOGGLE));
    lua_setfield(L, -2, "Key_KBDILLUMTOGGLE");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KBDILLUMDOWN));
    lua_setfield(L, -2, "Key_KBDILLUMDOWN");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_KBDILLUMUP));
    lua_setfield(L, -2, "Key_KBDILLUMUP");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_EJECT));
    lua_setfield(L, -2, "Key_EJECT");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_SLEEP));
    lua_setfield(L, -2, "Key_SLEEP");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_APP1));
    lua_setfield(L, -2, "Key_APP1");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_APP2));
    lua_setfield(L, -2, "Key_APP2");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_AUDIOREWIND));
    lua_setfield(L, -2, "Key_AUDIOREWIND");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_AUDIOFASTFORWARD));
    lua_setfield(L, -2, "Key_AUDIOFASTFORWARD");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_SOFTLEFT));
    lua_setfield(L, -2, "Key_SOFTLEFT");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_SOFTRIGHT));
    lua_setfield(L, -2, "Key_SOFTRIGHT");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_CALL));
    lua_setfield(L, -2, "Key_CALL");
    lua_pushinteger(L, static_cast<int>(InputScancode::Key_ENDCALL));
    lua_setfield(L, -2, "Key_ENDCALL");
    lua_pushinteger(L, static_cast<int>(InputScancode::SDL_NUM_SCANCODES));
    lua_setfield(L, -2, "SDL_NUM_SCANCODES");
    lua_setfield(L, -2, "InputScancode");

    LuaBinder<Debug> binder_Debug(L);
    binder_Debug.Begin("Debug")
        .BindStaticFn("DrawLine", &Debug::DrawLine) // void(float3 & from, float3 & to, float4 & color)
        .BindStaticFn("DrawBox", &Debug::DrawBox) // void(float3 & center, float3 & halfExtents, float4 & color)
        .End();

    LuaBinder<PhysicsHit> binder_PhysicsHit(L);
    binder_PhysicsHit.Begin("PhysicsHit")
        .BindMemFn("GetBody", &PhysicsHit::GetBody) // PhysicsBody*()
        .BindProperty("hasHit", &PhysicsHit::hasHit) // bool
        .BindProperty("point", &PhysicsHit::point) // float3
        .BindProperty("normal", &PhysicsHit::normal) // float3
        .BindProperty("distance", &PhysicsHit::distance) // float
        .End();

    LuaBinder<PhysicsOverlapResult> binder_PhysicsOverlapResult(L);
    binder_PhysicsOverlapResult.Begin("PhysicsOverlapResult")
        .BindMemFn("Count", &PhysicsOverlapResult::Count) // int()
        .BindMemFn("GetBody", &PhysicsOverlapResult::GetBody) // PhysicsBody*(int index)
        .End();

    LuaBinder<Physics> binder_Physics(L);
    binder_Physics.Begin("Physics")
        .BindStaticFn("RayCast", &Physics::RayCast) // PhysicsHit(float3 & origin, float3 & direction, float maxDistance)
        .BindStaticFn("SphereCast", &Physics::SphereCast) // PhysicsHit(float3 & origin, float radius, float3 & direction, float maxDistance)
        .BindStaticFn("BoxCast", &Physics::BoxCast) // PhysicsHit(float3 & origin, float3 & halfExtents, glm::quat & rotation, float3 & direction, float maxDistance)
        .BindStaticFn("CapsuleCast", &Physics::CapsuleCast) // PhysicsHit(float3 & origin, float halfHeight, float radius, glm::quat & rotation, float3 & direction, float maxDistance)
        .BindStaticFn("CheckSphere", &Physics::CheckSphere) // bool(float3 & center, float radius)
        .BindStaticFn("CheckBox", &Physics::CheckBox) // bool(float3 & center, float3 & halfExtents, glm::quat & rotation)
        .BindStaticFn("OverlapSphere", &Physics::OverlapSphere) // PhysicsOverlapResult(float3 & center, float radius)
        .BindStaticFn("OverlapBox", &Physics::OverlapBox) // PhysicsOverlapResult(float3 & center, float3 & halfExtents, glm::quat & rotation)
        .End();

    LuaBinder<RootMotionDelta> binder_RootMotionDelta(L);
    binder_RootMotionDelta.Begin("RootMotionDelta")
        .BindMemFn("IsEmpty", &RootMotionDelta::IsEmpty) // bool()
        .BindProperty("translation", &RootMotionDelta::translation) // glm::vec3
        .BindProperty("localTranslation", &RootMotionDelta::localTranslation) // glm::vec3
        .BindProperty("rotation", &RootMotionDelta::rotation) // glm::quat
        .BindProperty("localRotation", &RootMotionDelta::localRotation) // glm::quat
        .BindProperty("duration", &RootMotionDelta::duration) // float
        .BindProperty("hasTranslation", &RootMotionDelta::hasTranslation) // bool
        .BindProperty("hasRotation", &RootMotionDelta::hasRotation) // bool
        .End();

    LuaBinder<AnimationPlayer> binder_AnimationPlayer(L);
    binder_AnimationPlayer.Begin("AnimationPlayer")
        .BindMemFn("SetClip", &AnimationPlayer::SetClip) // bool(std::string & animationName)
        .BindMemFn("SetRootMotionEnabled", &AnimationPlayer::Lua_SetRootMotionEnabled) // void(bool enabled)
        .BindMemFn("SetRootMotionRoot", &AnimationPlayer::Lua_SetRootMotionRoot) // void(std::string rootName)
        .BindMemFn("SetRootMotionTranslationMode", &AnimationPlayer::Lua_SetRootMotionTranslationMode) // void(int mode)
        .BindMemFn("SetRootMotionRotationMode", &AnimationPlayer::Lua_SetRootMotionRotationMode) // void(int mode)
        .BindMemFn("GetRootMotionTranslationMode", &AnimationPlayer::Lua_GetRootMotionTranslationMode) // int()
        .BindMemFn("GetRootMotionRotationMode", &AnimationPlayer::Lua_GetRootMotionRotationMode) // int()
        .BindMemFn("PeekRootMotionDelta", &AnimationPlayer::PeekRootMotionDelta) // RootMotionDelta()
        .BindMemFn("ConsumeRootMotionDelta", &AnimationPlayer::ConsumeRootMotionDelta) // RootMotionDelta()
        .BindMemFn("Play", &AnimationPlayer::Play) // void()
        .BindMemFn("Stop", &AnimationPlayer::Stop) // void()
        .End();

    // Bind Enum RootMotionTranslationMode
    lua_newtable(L);
    lua_pushinteger(L, static_cast<int>(RootMotionTranslationMode::None));
    lua_setfield(L, -2, "None");
    lua_pushinteger(L, static_cast<int>(RootMotionTranslationMode::Horizontal));
    lua_setfield(L, -2, "Horizontal");
    lua_pushinteger(L, static_cast<int>(RootMotionTranslationMode::Vertical));
    lua_setfield(L, -2, "Vertical");
    lua_pushinteger(L, static_cast<int>(RootMotionTranslationMode::Full));
    lua_setfield(L, -2, "Full");
    lua_setfield(L, -2, "RootMotionTranslationMode");

    // Bind Enum RootMotionRotationMode
    lua_newtable(L);
    lua_pushinteger(L, static_cast<int>(RootMotionRotationMode::None));
    lua_setfield(L, -2, "None");
    lua_pushinteger(L, static_cast<int>(RootMotionRotationMode::Yaw));
    lua_setfield(L, -2, "Yaw");
    lua_pushinteger(L, static_cast<int>(RootMotionRotationMode::Full));
    lua_setfield(L, -2, "Full");
    lua_setfield(L, -2, "RootMotionRotationMode");

    LuaBinder<Boids> binder_Boids(L);
    binder_Boids.Begin("Boids")
        .End();

    LuaBinder<PhysicsBody> binder_PhysicsBody(L);
    binder_PhysicsBody.Begin("PhysicsBody")
        .BindMemFn("GetLinearVelocity", &PhysicsBody::GetLinearVelocity) // glm::vec3()
        .BindMemFn("AddForce", &PhysicsBody::AddForce) // void(glm::vec3 & force)
        .BindMemFn("AddImpulse", &PhysicsBody::AddImpulse) // void(glm::vec3 & impulse)
        .BindMemFn("SetGravityFactor", &PhysicsBody::SetGravityFactor) // void(float f)
        .End();

    LuaBinder<GameObject> binder_GameObject(L);
    binder_GameObject.Begin("GameObject")
        .BindMemFn("AddComponent", &GameObject::Lua_AddComponent) // Component*(char * componentName)
        .BindMemFn("GetComponentInHierarchy", &GameObject::GetComponentInHierarchy) // ObjPtr<Component>(char * className)
        .BindMemFn("SetName", &GameObject::Lua_SetName) // void(char * name)
        .BindMemFn("GetName", &GameObject::Lua_GetName) // std::string()
        .BindMemFn("GetScene", &GameObject::GetScene) // Scene*()
        .BindMemFn("IsEnabled", &GameObject::IsEnabled) // bool()
        .BindMemFn("IsActiveInScene", &GameObject::IsActiveInScene) // bool()
        .BindMemFn("SetEnable", &GameObject::SetEnable) // void(bool isEnabled)
        .BindMemFn("GetPosition", &GameObject::GetPosition) // float3()
        .BindMemFn("GetLocalPosition", &GameObject::GetLocalPosition) // float3()
        .BindMemFn("SetPosition", &GameObject::SetPosition) // void(float3 & position)
        .BindMemFn("SetLocalPosition", &GameObject::SetLocalPosition) // void(float3 & localPosition)
        .BindMemFn("GetRotation", &GameObject::GetRotation) // glm::quat()
        .BindMemFn("GetLocalRotation", &GameObject::GetLocalRotation) // glm::quat()
        .BindMemFn("SetRotation", &GameObject::SetRotation) // void(glm::quat & rotation)
        .BindMemFn("SetLocalRotation", &GameObject::SetLocalRotation) // void(glm::quat & rotation)
        .BindMemFn("GetEuluerAngles", &GameObject::GetEuluerAngles) // float3()
        .BindMemFn("SetEulerAngles", &GameObject::SetEulerAngles) // void(float3 & eulerAngles)
        .BindMemFn("LookAt", &GameObject::LookAt) // void(float3 & to)
        .BindMemFn("GetScale", &GameObject::GetScale) // float3()
        .BindMemFn("GetLocalScale", &GameObject::GetLocalScale) // float3()
        .BindMemFn("SetScale", &GameObject::SetScale) // void(float3 & scale)
        .BindMemFn("SetLocalScale", &GameObject::SetLocalScale) // void(float3 & scale)
        .BindMemFn("GetForward", &GameObject::GetForward) // float3()
        .BindMemFn("GetUp", &GameObject::GetUp) // float3()
        .BindMemFn("GetRight", &GameObject::GetRight) // float3()
        .BindMemFn("GetWorldMatrix", &GameObject::GetWorldMatrix) // float4x4()
        .BindMemFn("SetWorldMatrix", &GameObject::SetWorldMatrix) // void(float4x4 & model)
        .BindFn("GetComponent", &GameObject::LuaGetComponent) // ObjPtr<Component>(const char* className)
        .End();

    LuaBinder<Prefab> binder_Prefab(L);
    binder_Prefab.Begin("Prefab")
        .End();

    LuaBinder<Scene> binder_Scene(L);
    binder_Scene.Begin("Scene")
        .BindMemFn("CreateGameObject", &Scene::Lua_CreateGameObject) // ObjPtr<GameObject>()
        .BindMemFn("SpawnPrefab", &Scene::SpawnPrefab) // ObjPtr<GameObject>(ObjPtr<Prefab> & prefab)
        .BindMemFn("DestroyGameObject", &Scene::DestroyGameObject) // void(GameObject * obj)
        .End();

}