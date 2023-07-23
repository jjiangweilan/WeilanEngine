#pragma once
#include "Core/Scene/Scene.hpp"
#include <ThirdParty/imgui/imgui.h>

namespace Engine
{
class WeilanEngine;
}
namespace Engine::Editor
{

class GameEditor
{
public:
    GameEditor(WeilanEngine& engine);
    ~GameEditor();

    void Tick();
    void OnWindowResize(int32_t width, int32_t height);

private:
    WeilanEngine& engine;
    std::unique_ptr<GameObject> editorCameraGO;
    Camera* gameCamera;
    Camera* editorCamera;

    bool sceneTree = true;
    bool sceneInfo = false;
    void MainMenuBar();
};
} // namespace Engine::Editor
