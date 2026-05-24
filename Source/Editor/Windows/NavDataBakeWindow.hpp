#pragma once

#include "Editor/Window.hpp"
#include "Engine/Core/Ptr.hpp"

class Mesh;
class NavData;

namespace Editor
{
class NavDataBakeWindow : public Window
{
    DECLARE_EDITOR_WINDOW(NavDataBakeWindow)

public:
    bool Tick() override;

private:
    ObjPtr<Mesh> mesh;
    ObjPtr<NavData> navData;
};
} // namespace Editor
