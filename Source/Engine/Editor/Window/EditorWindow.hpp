#pragma once

#include "ThirdParty/imgui/imgui.h"

namespace Engine::Editor
{
class EditorWindow
{
public:
    virtual ~EditorWindow(){};
    // window event
    virtual void OnCreate() {}
    virtual void OnDestroy() {}
    virtual void Tick() {}

    // configuration function
    virtual const char* GetDisplayWindowName() { return GetWindowName(); };
    virtual const char* GetWindowName() = 0;
    virtual ImGuiWindowFlags GetWindowFlags() { return ImGuiWindowFlags_None; }
};
} // namespace Engine::Editor
