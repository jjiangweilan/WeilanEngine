#pragma once
#include "GfxDriver/GfxDriver.hpp"
#include "Rendering/RenderGraph/Graph.hpp"
#include "Rendering/Shaders.hpp"
#if ENGINE_EDITOR
#include "Editor/Renderer.hpp"
#endif
#include <memory>

namespace Engine
{
class Scene;
class RenderPipeline
{

public:
    RenderPipeline();
    void Render();
    void SetRenderGraph(
        std::unique_ptr<RenderGraph::Graph>&& graph,
        RenderGraph::RenderNode* swapchainOutputNode,
        RenderGraph::ResourceHandle swapchainOutputHandle,
        RenderGraph::RenderNode* depthOutputNode,
        RenderGraph::ResourceHandle depthOutputHandle
    );

private:
    // present data
    RefPtr<CommandQueue> queue;
    std::unique_ptr<Gfx::Semaphore> submitSemaphore;
    std::unique_ptr<Gfx::Fence> submitFence;
    std::unique_ptr<Gfx::Semaphore> swapchainAcquireSemaphore;
    std::unique_ptr<Gfx::CommandPool> commandPool;
    std::unique_ptr<Gfx::CommandBuffer> cmd;

    // graph
    std::unique_ptr<RenderGraph::Graph> graph;

#if ENGINE_EDITOR
    // editor rendering
    std::unique_ptr<Editor::Renderer> gameEditorRenderer;
#endif
};
}; // namespace Engine
