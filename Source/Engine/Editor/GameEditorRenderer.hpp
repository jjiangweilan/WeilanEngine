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
    void Render(CommandBuffer* cmdBuf, RGraph::ResourceStateTrack& stateTrack);

private:
    struct ImGuiData
    {
        UniPtr<Gfx::Buffer> vertexBuffer; // use single buffer to capture all the vertices
        UniPtr<Gfx::Buffer> indexBuffer;
        UniPtr<Gfx::Image> fontTex;
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
    RGraph::RenderGraph renderGraph;
    RGraph::RenderPassNode* gameEditorPass;
};
} // namespace Engine::Editor
