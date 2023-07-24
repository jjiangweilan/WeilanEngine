#pragma once
#include "Core/Scene/SceneManager.hpp"
#include "DualMoonRenderer.hpp"
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
    RenderPipeline(SceneManager& sceneManager);
    void Render();
    void RegisterSwapchainRecreateCallback(const std::function<void()>& callback)
    {
        this->swapchainRecreateCallback.push_back(callback);
    }

    DualMoonRenderer* GetRenderer()
    {
        return renderer.get();
    };

private:
    // present data
    RefPtr<CommandQueue> queue;
    std::unique_ptr<Gfx::Semaphore> submitSemaphore;
    std::unique_ptr<Gfx::Fence> submitFence;
    std::unique_ptr<Gfx::Semaphore> swapchainAcquireSemaphore;
    std::unique_ptr<Gfx::CommandPool> commandPool;
    std::unique_ptr<Gfx::CommandBuffer> cmd;
    std::unique_ptr<DualMoonRenderer> renderer;
    // graph
    std::vector<std::function<void()>> swapchainRecreateCallback;

#if ENGINE_EDITOR
    // editor rendering
    std::unique_ptr<Editor::Renderer> gameEditorRenderer;
#endif

    void BuildAndProcess();
    // void ProcessGraph(
    //     RenderGraph::RenderNode* swapchainOutputNode,
    //     RenderGraph::ResourceHandle swapchainOutputHandle,
    //     RenderGraph::RenderNode* depthOutputNode,
    //     RenderGraph::ResourceHandle depthOutputHandle
    // );
};
}; // namespace Engine
