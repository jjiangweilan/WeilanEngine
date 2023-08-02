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
    GameEditor(WeilanEngine& engine);
    ~GameEditor();

    void Tick();
    Rendering::CmdSubmitGroup GetCmdSubmitGroup();

    void OnWindowResize(int32_t width, int32_t height);

private:
    struct RegisteredTool
    {
        bool isOpen;
        Tool* tool;
    };

private:
    WeilanEngine& engine;
    std::unique_ptr<Editor::Renderer> gameEditorRenderer;
    std::unique_ptr<GameObject> editorCameraGO;
    std::unique_ptr<Gfx::CommandBuffer> cmd;
    std::unique_ptr<Gfx::CommandPool> cmdPool;
    Camera* gameCamera;
    Camera* editorCamera;

    bool sceneTree = true;
    bool sceneInfo = false;

    std::vector<RegisteredTool> registeredTools;
    void MainMenuBar();
    void OpenWindow();

    std::vector<std::unique_ptr<Tool>> toolList;
};
} // namespace Engine::Editor
