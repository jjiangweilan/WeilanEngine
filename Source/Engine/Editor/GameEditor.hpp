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
    void OnWindowResize(int32_t width, int32_t height);

private:
    Scene& scene;
    std::unique_ptr<GameObject> editorCameraGO;
    Camera* gameCamera;
    Camera* editorCamera;
};
} // namespace Engine::Editor
