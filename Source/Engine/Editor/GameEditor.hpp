#pragma once

#include "Core/AssetDatabase/AssetDatabase.hpp"
#include "Core/GameObject.hpp"
#include "Core/GameScene/GameScene.hpp"
#include "GameEditorRenderer.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Libs/Ptr.hpp"
#include "ProjectManagement/ProjectManagement.hpp"
#include "ThirdParty/imgui/imgui.h"
#include "Window/AssetExplorer.hpp"
#include "Window/GameSceneWindow.hpp"
#include "Window/InspectorWindow.hpp"
#include "Window/ProjectManagementWindow.hpp"
#include "Window/ProjectWindow.hpp"
#include "Window/SceneTreeWindow.hpp"
#include <SDL2/SDL.h>
#include <memory>

namespace Engine::Editor
{
class GameEditor
{
public:
    GameEditor(RefPtr<Gfx::GfxDriver> gfxDriver, RefPtr<ProjectManagement> projectManagement);
    ~GameEditor();
    void Init();

    void ChangeWindowSize();
    void ProcessEvent(const SDL_Event& event);
    bool IsProjectInitialized();
    void Tick();
    void BuildRenderGraph(RGraph::RenderGraph* graph, RGraph::Port* finalColorPort, RGraph::Port* finalDepthPort);
    GameEditorRenderer* GetGameEditorRenderer() { return gameEditorRenderer.Get(); }

private:
    struct WindowInfo
    {
        UniPtr<EditorWindow> window;
        std::size_t typeHash;
        int id;
    };

    template <class T>
    EditorWindow* CreateEditorWindow()
    {
        T* win = new T(editorContext);
        const std::type_info& typeInfo = typeid(T);
        std::size_t hashCode = typeInfo.hash_code();

        // get a window ID for type T
        int windowID = -1;
        std::vector<int>& recycledID = windowIDRecycle[hashCode];
        if (!recycledID.empty())
        {
            windowID = recycledID.back();
            recycledID.pop_back();
        }
        else
        {
            int& c = windowTypeToCount[hashCode];
            windowID = c;
            c++;
        }

        windows.push_back({UniPtr<EditorWindow>(win), hashCode, windowID});

        win->OnCreate();
        return win;
    }

    void DestroyEditorWindow(WindowInfo& winInfo)
    {
        EditorWindow* window = winInfo.window.Get();
        auto iter =
            std::find_if(windows.begin(), windows.end(), [window](const auto& p) { return p.window.Get() == window; });

        if (iter != windows.end())
        {
            (*iter).window->OnDestroy();

            // recycle the window id
            windowIDRecycle[winInfo.typeHash].push_back(winInfo.id);
            windows.erase(iter);
        }
        else
        {
            SPDLOG_ERROR("Destroy an window that is not created by CreateEditorWindow");
        }
    }

    /* Data */
    RefPtr<ProjectManagement> projectManagement;

    /* Windows */
    UniPtr<EditorContext> editorContext;
    UniPtr<SceneTreeWindow> sceneTreeWindow;
    UniPtr<ProjectWindow> projectWindow;
    UniPtr<AssetExplorer> assetExplorer;
    UniPtr<InspectorWindow> inspector;
    UniPtr<GameSceneWindow> gameSceneWindow;
    UniPtr<ProjectManagementWindow> projectManagementWindow;

    /* Rendering Structures */
    RefPtr<Gfx::GfxDriver> gfxDriver;
    UniPtr<Gfx::RenderPass> editorPass;
    RefPtr<Gfx::Image> gameColorImage;
    RefPtr<Gfx::Image> gameDepthImage;
    void DrawMainMenu();
    void RenderEditor(RefPtr<CommandBuffer> cmdBuf);
    UniPtr<GameEditorRenderer> gameEditorRenderer;

    UniPtr<Gfx::ShaderResource> res;
    UniPtr<Gfx::RenderPass> renderPass;

    std::vector<WindowInfo> windows;
    std::unordered_map<std::size_t, int> windowTypeToCount;
    std::unordered_map<std::size_t, std::vector<int>> windowIDRecycle;
};
} // namespace Engine::Editor
