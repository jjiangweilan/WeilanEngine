#pragma once
#include "Core/Scene/Scene.hpp"
#include "Renderer.hpp"
#include "Rendering/CmdSubmitGroup.hpp"
#include <ThirdParty/imgui/imgui.h>

namespace Engine
{
class WeilanEngine;
}
namespace Engine::Editor
{

class Tool;
class GameEditor
{
public:
    GameEditor(const char* path);
    ~GameEditor();

    void Start();

    void OnWindowResize(int32_t width, int32_t height);

private:
    struct RegisteredTool
    {
        bool isOpen;
        Tool* tool;
    };

private:
    std::unique_ptr<WeilanEngine> engine;
    std::unique_ptr<Editor::Renderer> gameEditorRenderer;
    std::unique_ptr<GameObject> editorCameraGO;
    Camera* gameCamera;
    Camera* editorCamera;

    bool sceneTree = true;
    bool sceneInfo = false;

    std::vector<RegisteredTool> registeredTools;
    std::vector<std::unique_ptr<Tool>> toolList;

    void MainMenuBar();
    void OpenWindow();
    void GUIPass();
    void Render(Gfx::CommandBuffer& cmd);
};
} // namespace Engine::Editor
