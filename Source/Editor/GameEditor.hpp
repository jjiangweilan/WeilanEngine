#pragma once
#include "Core/Scene/Scene.hpp"
#include "FrameGraph/FrameGraphEditor.hpp"
#include "Renderer.hpp"
#include "Rendering/CmdSubmitGroup.hpp"
#include "ThirdParty/imgui/imgui.h"
#include "Tools/GameView.hpp"
#include "WeilanEngine.hpp"
#include "Window.hpp"
#include <spdlog/sinks/ringbuffer_sink.h>
#include <spdlog/spdlog.h>

class WeilanEngine;
namespace Editor
{

class Tool;
class InspectorBase;
class GameEditor
{
public:
    GameEditor(const char* path);
    ~GameEditor();

    void Start();
    WeilanEngine* GetEngine()
    {
        return engine.get();
    }

    std::unique_ptr<Gfx::Image> fontImage;
    nlohmann::json editorConfig;
    static GameEditor* instance;

private:
    struct RegisteredTool
    {
        bool isOpen;
        Tool* tool;
    };

private:
    std::unique_ptr<WeilanEngine> engine;
    std::unique_ptr<Editor::Renderer> gameEditorRenderer;
    GameLoop* loop;

    GameView gameView;

    GameObject* sceneTreeContextObject = nullptr;
    bool beginSceneTreeContextPopup = false;
    bool sceneTree = true;
    bool sceneInfo = false;

    bool assetWindow = true;
    bool inspectorWindow = true;
    bool openSceneWindow = false;
    bool createSceneWindow = false;
    bool surfelGIBaker = false;

    InspectorBase* primaryInspector = nullptr;
    InspectorBase* secondaryInspector = nullptr;

    std::vector<RegisteredTool> registeredTools;
    std::vector<std::unique_ptr<Tool>> toolList;
    std::unique_ptr<Gfx::CommandBuffer> cmd;
    std::list<std::unique_ptr<Window>> activeWindows;

    void EnableMultiViewport();

    void OpenSceneWindow();
    void MainMenuBar();
    void OpenWindow();
    void GUIPass();
    void Render(
        Gfx::CommandBuffer& cmd,
        const Gfx::RG::ImageIdentifier* gameImage,
        const Gfx::RG::ImageIdentifier* gameDepthImage
    );

    void AssetWindow();
    void AssetShowDir(const std::filesystem::path& path);
    void InspectorWindow();
    void SurfelGIBakerWindow();

    void ConsoleOutputWindow();

    void AddPrimitiveAssetToScene(Scene& scene, std::string_view path);
    void SceneTree(Scene& scene);
    void SceneTree(GameObject* go, int imguiID, GameObject* currentSelected, bool autoExpand);

    void WindowRegisteryIteration(WindowRegisterInfo& info, int pathIndex);
};
} // namespace Editor
