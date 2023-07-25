#pragma once
#include "GfxDriver/GfxDriver.hpp"
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
    void Render(Scene* scene);
    void RegisterSwapchainRecreateCallback(const std::function<void()>& callback)
    {
        this->swapchainRecreateCallback.push_back(callback);
    }

private:
    // present data
    RefPtr<CommandQueue> queue;
    std::unique_ptr<Gfx::Semaphore> submitSemaphore;
    std::unique_ptr<Gfx::Fence> submitFence;
    std::unique_ptr<Gfx::Semaphore> swapchainAcquireSemaphore;
    std::unique_ptr<Gfx::CommandPool> commandPool;
    std::unique_ptr<Gfx::CommandBuffer> cmd;
    std::vector<std::function<void()>> swapchainRecreateCallback;

#if ENGINE_EDITOR
    // editor rendering
    std::unique_ptr<Editor::Renderer> gameEditorRenderer;
#endif

    void BuildAndProcess();
};
}; // namespace Engine
