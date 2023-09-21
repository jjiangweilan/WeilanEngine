#pragma once
#include "Core/Scene/Scene.hpp"
#include "Renderer.hpp"
#include "Rendering/CmdSubmitGroup.hpp"
#include "Tools/GameView.hpp"
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

    GameView gameView;

    Camera* gameCamera;
    Camera* editorCamera;

    bool sceneTree = true;
    bool sceneInfo = false;

    bool assetWindow = true;
    bool inspectorWindow = true;

    std::vector<RegisteredTool> registeredTools;
    std::vector<std::unique_ptr<Tool>> toolList;

    void OpenSceneWindow();
    void MainMenuBar();
    void OpenWindow();
    void GUIPass();
    void Render(Gfx::CommandBuffer& cmd);

    void AssetWindow();
    void AssetShowDir(const std::filesystem::path& path);

    void InspectorWindow();

    void SceneTree(Scene& scene);
    void SceneTree(Transform* transform);
};
} // namespace Engine::Editor
