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

void RenderPipeline::Render(Rendering::CmdSubmitGroup& submitGroup)
{

    GetGfxDriver()->WaitForFence({submitFence}, true, -1);
    submitFence->Reset();
    if (GetGfxDriver()->AcquireNextSwapChainImage(swapchainAcquireSemaphore))
    {
        swapchainAcquireSemaphore = GetGfxDriver()->CreateSemaphore({.signaled = false});
        if (GetGfxDriver()->AcquireNextSwapChainImage(swapchainAcquireSemaphore))
        {
            throw std::runtime_error("the second acquire swapchain image is not successful.");
        }

        for (auto& cb : swapchainRecreateCallback)
        {
            cb();
        }
    }

    cmdQueue.clear();
    cmdQueue.insert(cmdQueue.end(), submitGroup.GetCmds().begin(), submitGroup.GetCmds().end());

    RefPtr<Gfx::Semaphore> waitSemaphores[] = {swapchainAcquireSemaphore};
    Gfx::PipelineStageFlags waitPipelineStages[] = {Gfx::PipelineStage::Color_Attachment_Output};
    RefPtr<Gfx::Semaphore> submitSemaphores[] = {submitSemaphore.get()};
    GetGfxDriver()->QueueSubmit(queue, cmdQueue, waitSemaphores, waitPipelineStages, submitSemaphores, submitFence);

    GetGfxDriver()->Present({submitSemaphore});
}
} // namespace Engine
