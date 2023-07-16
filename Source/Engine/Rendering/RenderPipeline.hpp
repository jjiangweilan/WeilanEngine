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
    RenderPipeline(Scene* scene = nullptr, Shader* sceneObjectShader = nullptr);
    void Render();

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
    Scene* scene;

#if ENGINE_EDITOR
    // editor rendering
    std::unique_ptr<Editor::Renderer> gameEditorRenderer;
#endif
};
}; // namespace Engine
