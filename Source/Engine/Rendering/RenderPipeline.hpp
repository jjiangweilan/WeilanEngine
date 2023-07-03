#pragma once
#include "GfxDriver/GfxDriver.hpp"
#include <memory>

namespace Engine
{
class RenderPipeline
{

public:
    RenderPipeline();
    void Draw();

private:
    RefPtr<CommandQueue> queue;
    std::unique_ptr<Gfx::Semaphore> submitSemaphore;
    std::unique_ptr<Gfx::Fence> submitFence;
    std::unique_ptr<Gfx::Semaphore> swapchainAcquireSemaphore;
    std::unique_ptr<Gfx::CommandPool> commandPool;
    std::unique_ptr<CommandBuffer> cmd;
    bool present;
};
}; // namespace Engine
