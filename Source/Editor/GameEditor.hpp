#pragma once
#include "Core/Scene/Scene.hpp"
#include "FrameGraph/FrameGraphEditor.hpp"
#include "Profiler/Profiler.hpp"
#include "Renderer.hpp"
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
    bool assetDatabaseWindow = false;
    bool pbrBaker = false;

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
    void AssetDatabaseViewer();

    void AddPrimitiveAssetToScene(Scene& scene, std::string_view path);
    void SceneTree(Scene& scene);
    void SceneTree(
        GameObject* go, int imguiID, GameObject* currentSelected, std::vector<SRef<Object>>& selects, bool autoExpand
    );
    void GameProfiler(Profiler& profiler);

    void WindowRegisteryIteration(WindowRegisterInfo& info, int pathIndex);

    // event handling
    class EndEvents
    {
    public:
        void TickBegin()
        {
            fs.clear();
        }

        void Register(const std::function<void()>& f)
        {
            fs.push_back(f);
        }

        void TickEnd()
        {
            for (auto& f : fs)
            {
                f();
            }
        }

    private:
        std::vector<std::function<void()>> fs;
    } endEvents;

    class EndPopup
    {
        std::string text;
        std::function<void()> f;
        bool show;

    public:
        void TickBegin()
        {
            show = false;
        }

        void Show(const std::string& text, const std::function<void()>& confirm)
        {
            this->text = text;
            show = true;
            f = confirm;
        }

        void TickEnd()
        {
            if (show)
            {
                ImGui::OpenPopup("Tick End Popup");
            }

            if (ImGui::BeginPopupModal("Tick End Popup"))
            {
                ImGui::Text("%s", text.c_str());

                if (ImGui::Selectable("Confirm"))
                {
                    f();
                }
                if (ImGui::Selectable("Chancel"))
                {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }
    } endPopup;
};
} // namespace Editor
