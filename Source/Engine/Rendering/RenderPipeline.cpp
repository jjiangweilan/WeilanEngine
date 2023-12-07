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
    if (result == Gfx::AcquireNextSwapChainImageResult::Succeeded)
        return true;

    return false;
}

void RenderPipeline::Schedule(const std::function<void(Gfx::CommandBuffer& cmd)>& f)
{
    pendingWorks.push_back(std::move(f));
}

void RenderPipeline::Render()
{
    if (!AcquireSwapchainImage())
        return;

    GetGfxDriver()->WaitForFence({submitFence}, true, -1);
    GetGfxDriver()->ClearResources();
    submitFence->Reset();

    Gfx::CommandBuffer* cmd = frameCmdBuffer.GetActive();
    cmd->Begin();

    for (auto& f : pendingWorks)
    {
        f(*cmd);
    }
    cmd->End();

    cmdQueue.clear();
    cmdQueue.push_back(cmd);

    RefPtr<Gfx::Semaphore> waitSemaphores[] = {swapchainAcquireSemaphore};
    Gfx::PipelineStageFlags waitPipelineStages[] = {Gfx::PipelineStage::Color_Attachment_Output};
    RefPtr<Gfx::Semaphore> submitSemaphores[] = {submitSemaphore.get()};
    GetGfxDriver()->QueueSubmit(queue, cmdQueue, waitSemaphores, waitPipelineStages, submitSemaphores, submitFence);
    GetGfxDriver()->Present({submitSemaphore});

    pendingWorks.clear();
    frameCmdBuffer.Swap();
}

RenderPipeline::FrameCmdBuffer::FrameCmdBuffer()
{
    auto queueFamilyIndex = GetGfxDriver()->GetQueue(QueueType::Main)->GetFamilyIndex();
    cmdPool = GetGfxDriver()->CreateCommandPool({queueFamilyIndex});

    cmd0 = cmdPool->AllocateCommandBuffers(Gfx::CommandBufferType::Primary, 1)[0];
    cmd1 = cmdPool->AllocateCommandBuffers(Gfx::CommandBufferType::Primary, 1)[0];

    activeCmd = cmd0.get();
}

std::unique_ptr<RenderPipeline>& RenderPipeline::SingletonPrivate()
{
    static std::unique_ptr<RenderPipeline> instance = nullptr;
    return instance;
}

void RenderPipeline::Init()
{
    SingletonPrivate() = std::unique_ptr<RenderPipeline>(new RenderPipeline());
}

void RenderPipeline::Deinit()
{
    SingletonPrivate() = nullptr;
}

void RenderPipeline::FrameCmdBuffer::Swap()
{
    if (activeCmd == cmd0.get())
    {
        activeCmd = cmd1.get();
    }
    else
    {
        activeCmd = cmd0.get();
    }

    // the newly actived will be reset here
    activeCmd->Reset(false);
}

RenderPipeline& RenderPipeline::Singleton()
{
    return *SingletonPrivate();
}

void RenderPipeline::SetBufferData(Gfx::Buffer& dst, uint8_t* data, size_t size, size_t dstOffset)
{
    pendingSetBuffers.push_back({});
}
} // namespace Engine
