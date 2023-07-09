#pragma once
#include "GfxDriver/GfxDriver.hpp"
#include "Rendering/RenderGraph/Graph.hpp"
#include <memory>

namespace Engine
{
class RenderPipeline
{

public:
    RenderPipeline();
    void Render(RenderGraph::Graph& graph);

private:
    RefPtr<CommandQueue> queue;
    std::unique_ptr<Gfx::Semaphore> submitSemaphore;
    std::unique_ptr<Gfx::Fence> submitFence;
    std::unique_ptr<Gfx::Semaphore> swapchainAcquireSemaphore;
    std::unique_ptr<Gfx::CommandPool> commandPool;
    std::unique_ptr<Gfx::CommandBuffer> cmd;
    bool present;
};
}; // namespace Engine
