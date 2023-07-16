#pragma once
#include "Core/Scene/Scene.hpp"
#include <ThirdParty/imgui/imgui.h>

namespace Engine::Editor
{

class GameEditor
{
public:
    GameEditor(Scene& scene);
    ~GameEditor();

    void Tick();

private:
    Scene& scene;
};
} // namespace Engine::Editor
