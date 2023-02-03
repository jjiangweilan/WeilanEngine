#pragma once

#include "ThirdParty/imgui/imgui.h"

namespace Engine::Editor
{
class EditorWindow
{
public:
    ~EditorWindow(){};
    // window event
    virtual void OnCreate() {}
    virtual void OnDestroy() {}
    virtual void Tick() {}

    // configuration function
    virtual const char* GetWindowName() = 0;
    virtual ImGuiWindowFlags_ GetWindowFlags() { return ImGuiWindowFlags_None; }
};
} // namespace Engine::Editor
