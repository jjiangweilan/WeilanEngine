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
};
} // namespace Engine::Editor
