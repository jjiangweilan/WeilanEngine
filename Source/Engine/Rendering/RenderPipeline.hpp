#pragma once
#include "CmdSubmitGroup.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include <memory>
#include <functional>
namespace Engine
{
class Scene;
class RenderPipeline
{

public:
    RenderPipeline();

    // this function makes sure the previously submiited commands are finished
    void WaitForPreviousFrame();
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
