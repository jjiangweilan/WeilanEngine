#pragma once
#include "Editor/AssetBrowser.hpp"
#include "Editor/EditorContext.hpp"
#include "Editor/GameView.hpp"
#include "Editor/Renderer.hpp"
#include "Editor/Window.hpp"
#include "Engine/Core/Profiler/Profiler.hpp"
#include "Engine/Core/Ptr.hpp"
#include "Engine/Runtime/System/SceneManager/Scene.hpp"
#include "Engine/ThirdParty/imgui/imgui.h"
#include "Engine/ThirdParty/imgui/imgui_internal.h"
#include "EngineCommandGUI.hpp"
#include "SceneEditor.hpp"
#include <spdlog/sinks/ringbuffer_sink.h>
#include <spdlog/spdlog.h>

class WeilanEngine;
namespace Editor
{

class InspectorBase;
class AssetBrowser;
class GameEditor
{
    friend class AssetBrowser; // Allow AssetBrowser to access private members

public:
    GameEditor(WeilanEngine* engine, const char* path);
    ~GameEditor();

    bool IsGameViewVisible();
    float2 GetGameScreenSize();
    // void Start(); remove
    void Tick();
    void AfterGameLoopTick();
    void Render(
        Gfx::CommandBuffer& cmd,
        const Gfx::ImageIdentifier* gameImage,
        const Gfx::ImageIdentifier* gameDepthImage
    );
    WeilanEngine* GetEngine() { return engine; }

    void SetActiveScene(ObjPtr<Scene> scene);

    std::unique_ptr<Gfx::Image> fontImage;
    nlohmann::json editorState;
    static GameEditor* instance;

    EditorContext* GetEditorContext() { return editorContext.get(); }

private:
    void EnableMultiViewport();

    void ShowSceneWindow();
    void MainMenuBar();
    void OpenWindow();
    void GUIPass();
    void SaveProject();
    void SimulatePlayerView(bool enable);

    void ShowInspectorWindow();
    void ShowSurfelGIBakerWindow();
    void ShowConsoleOutputWindow();
    void ShowAssetDatabaseViewer();
    void ShowRenderPipelineSetting();
    void ShowStaticEngineDebugs();
    void AddPrimitiveAssetToScene(Scene& scene, std::string_view path);
    void ShowSceneTree(Scene& scene);
    void SceneTree(
        GameObject* go, int imguiID, GameObject* currentSelected, std::vector<ObjPtr<Object>>& selects, bool autoExpand
    );
    void ShowGameProfiler(IProfiler& profiler);
    void ShowEngineResourceDebug();

    void WindowRegisteryIteration(WindowRegisterInfo& info, int pathIndex);

    std::unique_ptr<EditorContext> editorContext = std::make_unique<EditorContext>();
    std::unique_ptr<GizmoManager> gizmoManager;

    std::string imguiInitPath;
    WeilanEngine* engine;
    std::unique_ptr<Editor::Renderer> gameEditorRenderer;

    std::unique_ptr<GameView> gameView;
    std::unique_ptr<SceneEditor> sceneEditor;
    std::unique_ptr<AssetBrowser> assetBrowser;
    std::unique_ptr<EngineCommandGUI> engineCommandGUI;

    GameObject* sceneViewHightedGameObjectCandidate = nullptr;
    GameObject* sceneTreeContextObject = nullptr;
    bool beginSceneTreeContextPopup = false;
    bool sceneTree = true;
    bool sceneInfo = false;

    bool hideDevTool = false;
    bool engineDebug = true;
    bool assetWindow = true;
    bool inspectorWindow = true;
    bool openSceneWindow = false;
    bool createSceneWindow = false;
    bool surfelGIBaker = false;
    bool assetDatabaseWindow = false;
    bool pbrBaker = false;
    bool debugEngineResources = false;

    InspectorBase* primaryInspector = nullptr;
    InspectorBase* secondaryInspector = nullptr;

    std::list<std::unique_ptr<Window>> activeWindows;

    int2 cacheSystemWindowSize;

public:
    // event handling
    class EndEvents
    {
    public:
        void TickBegin() { fs.clear(); }

        void Register(const std::function<void()>& f) { fs.push_back(f); }

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
        std::function<void()> cancel;
        bool show;
        bool open = false;

    public:
        void TickBegin() { show = false; }

        void Show(
            const std::string& text, const std::function<void()>& confirm, const std::function<void()>& cancel = nullptr
        )
        {
            this->text = text;
            this->cancel = cancel;
            show = true;
            f = confirm;
            open = true;
        }

        void TickEnd()
        {
            if (show)
            {
                ImGui::OpenPopup("Tick End Popup");
            }

            if (ImGui::BeginPopupModal("Tick End Popup", &open))
            {
                ImGui::Text("%s", text.c_str());

                if (ImGui::Button("Yes"))
                {
                    f();
                }
                ImGui::SameLine();
                if (ImGui::Button("No"))
                {
                    if (cancel != nullptr)
                        cancel();
                    else
                        ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }
    } endPopup;
};
} // namespace Editor
