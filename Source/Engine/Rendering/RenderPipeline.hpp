#pragma once
#include "CmdSubmitGroup.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include <functional>
#include <memory>
namespace Engine
{
class Scene;
class RenderPipeline
{

public:
    RenderPipeline();

    void AcquireSwapchainImage();
    void Render(Rendering::CmdSubmitGroup& submitGroup);
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
    std::vector<std::function<void()>> swapchainRecreateCallback;

    std::vector<Gfx::CommandBuffer*> cmdQueue;

    void BuildAndProcess();
};
}; // namespace Engine
