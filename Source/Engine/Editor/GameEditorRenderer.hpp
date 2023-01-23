#pragma once
#include "Core/Rendering/RenderGraph/RenderGraph.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "ThirdParty/imgui/imgui.h"

namespace Engine::Editor
{
class GameEditorRenderer
{
public:
    GameEditorRenderer();
    ~GameEditorRenderer() { AssetDatabase::Instance()->UnregisterOnAssetReload(assetReloadIterHandle); }
    void Render();
    void Tick();
    void SetGameSceneImageTarget(Gfx::Image* gameSceneImageTarget)
    {
        this->gameSceneImageTarget = gameSceneImageTarget;
    };

    /* return the port outputing the game editor's color target
     */
    RGraph::Port* BuildGraph(RGraph::RenderGraph* graph, RGraph::Port* finalColorPort, RGraph::Port* finalDepthPort);

private:
    struct ImGuiData
    {
        UniPtr<Gfx::Buffer> vertexBuffer; // use single buffer to capture all the vertices
        UniPtr<Gfx::Buffer> indexBuffer;
        UniPtr<Texture> fontTex;
        UniPtr<Gfx::Image> editorRT;
        UniPtr<Gfx::ShaderResource> generalShaderRes;
        RefPtr<Gfx::ShaderProgram> shaderProgram;
        Gfx::ShaderConfig shaderConfig;

        RefPtr<Gfx::ShaderResource> GetImageResource();
        void ResetImageResource() { currCacheIndex = -1; }
        void ClearImageResource();

    private:
        int currCacheIndex = -1;
        std::vector<UniPtr<Gfx::ShaderResource>> imageResourcesCache;
    };
    AssetDatabase::OnAssetReloadIterHandle assetReloadIterHandle;

    ImGuiData imGuiData;
    RGraph::RenderGraph* graph;
    RGraph::Port* finalColorPort;
    RGraph::Port* finalDepthPort;
    RGraph::RenderPassNode* gameEditorPassNode;
    RGraph::DrawList drawList;
    Gfx::Image* gameSceneImageTarget;
};
} // namespace Engine::Editor
