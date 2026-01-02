// GENERATED FILE - DO NOT EDIT
#include "Engine/Runtime/System/ScriptingBackend/LuaBindings.hpp"
#include "Engine/Runtime/System/ScriptingBackend/LuaBindings_Private.hpp"

#include "Engine/Game/Input.hpp"
#include "Engine/Runtime/Object/Component/AnimationPlayer.hpp"
#include "Engine/Runtime/Object/GameObject/GameObject.hpp"

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

    LuaBinder<AnimationPlayer> binder_AnimationPlayer(L);
    binder_AnimationPlayer.Begin("AnimationPlayer")
        .BindMemFn("SetClip", &AnimationPlayer::SetClip) // bool(std::string & animationName)
        .BindMemFn("Play", &AnimationPlayer::Play) // void()
        .End();

    LuaBinder<GameObject> binder_GameObject(L);
    binder_GameObject.Begin("GameObject")
        .BindMemFn("GetComponentInHierachy", &GameObject::GetComponentInHierachy) // ObjPtr<Component>(char * className)
        .BindMemFn("GetPosition", &GameObject::GetPosition) // glm::vec3()
        .BindMemFn("SetPosition", &GameObject::SetPosition) // void(glm::vec3 & position)
        .BindMemFn("LookAt", &GameObject::LookAt) // void(glm::vec3 & to)
        .BindFn("GetComponent", &GameObject::LuaGetComponent)
        .End();

}