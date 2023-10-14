#include "RenderPipeline.hpp"
#include "Core/Scene/Scene.hpp"

namespace Engine
{

RenderPipeline::RenderPipeline()
{
    submitFence = GetGfxDriver()->CreateFence({.signaled = true});
    submitSemaphore = GetGfxDriver()->CreateSemaphore({.signaled = false});
    swapchainAcquireSemaphore = GetGfxDriver()->CreateSemaphore({.signaled = false});
    queue = GetGfxDriver()->GetQueue(QueueType::Main).Get();

#if ENGINE_EDITOR
#endif
    /// auto [swapchainNode, swapchainHandle, depthNode, depthHandle] = renderer->GetFinalSwapchainOutputs();
    // ProcessGraph(swapchainNode, swapchainHandle, depthNode, depthHandle);
}

bool RenderPipeline::AcquireSwapchainImage()
{
    auto result = GetGfxDriver()->AcquireNextSwapChainImage(swapchainAcquireSemaphore);
    if (result == Gfx::AcquireNextSwapChainImageResult::Recreated)
    {
        for (auto& cb : swapchainRecreateCallback)
        {
            cb();
        }
    }
    else if (result == Gfx::AcquireNextSwapChainImageResult::Failed)
        return false;
    else if (result == Gfx::AcquireNextSwapChainImageResult::Succeeded)
        return true;

    return false;
}

void RenderPipeline::Render(Rendering::CmdSubmitGroup& submitGroup)
{
    GetGfxDriver()->WaitForFence({submitFence}, true, -1);
    submitFence->Reset();

    cmdQueue.clear();
    cmdQueue.insert(cmdQueue.end(), submitGroup.GetCmds().begin(), submitGroup.GetCmds().end());

    RefPtr<Gfx::Semaphore> waitSemaphores[] = {swapchainAcquireSemaphore};
    Gfx::PipelineStageFlags waitPipelineStages[] = {Gfx::PipelineStage::Color_Attachment_Output};
    RefPtr<Gfx::Semaphore> submitSemaphores[] = {submitSemaphore.get()};
    GetGfxDriver()->QueueSubmit(queue, cmdQueue, waitSemaphores, waitPipelineStages, submitSemaphores, submitFence);

    GetGfxDriver()->Present({submitSemaphore});
}
} // namespace Engine
