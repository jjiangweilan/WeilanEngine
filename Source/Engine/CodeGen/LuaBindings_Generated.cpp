// GENERATED FILE - DO NOT EDIT
#include "LuaBindings.hpp"
#include "LuaBindings_Private.hpp"

#include "Engine/Game/Input.hpp"
#include "Engine/Runtime/Object/Component/AnimationPlayer.hpp"

void BindGeneratedClasses(lua_State* L)
{
    LuaBinder<Gamepad> binder_Gamepad(L);
    binder_Gamepad.Begin("Gamepad")
        .BindMemFn("IsButtonPressed", &Gamepad::IsButtonPressed)
        .BindMemFn("IsBumperPressed", &Gamepad::IsBumperPressed)
        .BindMemFn("GetTrigger", &Gamepad::GetTrigger)
        .BindMemFn("GetAxis", &Gamepad::GetAxis)
        .End();

    LuaBinder<Input> binder_Input(L);
    binder_Input.Begin("Input")
        .BindStaticFn("GetGamepad", &Input::GetGamepad)
        .BindStaticFn("GetMovementX", &Input::GetMovementX)
        .BindStaticFn("GetMovementY", &Input::GetMovementY)
        .BindStaticFn("IsInteractPressed", &Input::IsInteractPressed)
        .BindStaticFn("GetLookAroundX", &Input::GetLookAroundX)
        .BindStaticFn("GetLookAroundY", &Input::GetLookAroundY)
        .BindStaticFn("Jump", &Input::Jump)
        .End();

    LuaBinder<AnimationPlayer> binder_AnimationPlayer(L);
    binder_AnimationPlayer.Begin("AnimationPlayer")
        .BindMemFn("SetClip", &AnimationPlayer::SetClip)
        .BindMemFn("Play", &AnimationPlayer::Play)
        .End();

}