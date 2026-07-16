#pragma once

#include "Editor/Window.hpp"
#include "Engine/Core/Ptr.hpp"
#include <string>

class NavData;
class GameObject;

namespace Editor
{
class NavDataBakeWindow : public Window
{
    DECLARE_EDITOR_WINDOW(NavDataBakeWindow)

public:
    bool Tick() override;

private:
    ObjPtr<GameObject> rootObject;
    ObjPtr<NavData> navData;
    std::string statusMessage;
};
} // namespace Editor
