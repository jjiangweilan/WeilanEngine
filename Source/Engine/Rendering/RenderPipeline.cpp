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

void RenderPipeline::Schedule(std::function<void(Gfx::CommandBuffer& cmd)>&& f)
{
    pendingWorks.push_back(std::move(f));
}

void RenderPipeline::Render()
{
    if (!AcquireSwapchainImage())
        return;

    Gfx::CommandBuffer* cmd = frameCmdBuffer.GetActive();
    cmd->Begin();

    staging.UploadBuffers(*cmd);

    for (auto& f : pendingWorks)
    {
        f(*cmd);
    }
    cmd->End();

    cmdQueue.clear();
    cmdQueue.push_back(cmd);

    GetGfxDriver()->WaitForFence({submitFence}, true, -1);
    submitFence->Reset();

    RefPtr<Gfx::Semaphore> waitSemaphores[] = {swapchainAcquireSemaphore};
    Gfx::PipelineStageFlags waitPipelineStages[] = {Gfx::PipelineStage::Color_Attachment_Output};
    RefPtr<Gfx::Semaphore> submitSemaphores[] = {submitSemaphore.get()};
    GetGfxDriver()->QueueSubmit(queue, cmdQueue, waitSemaphores, waitPipelineStages, submitSemaphores, submitFence);
    GetGfxDriver()->Present({submitSemaphore});

    pendingWorks.clear();
    frameCmdBuffer.Swap();

    GetGfxDriver()->ClearResources();
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

void RenderPipeline::UploadBuffer(Gfx::Buffer& dst, uint8_t* data, size_t size, size_t dstOffset)
{
    staging.UploadBuffer(dst, data, size, dstOffset);
}

void RenderPipeline::StagingBuffer::UploadBuffer(Gfx::Buffer& dst, uint8_t* data, size_t size, size_t dstOffset)
{
    const size_t stagingBufferSize = staging->GetSize();

    if (stagingOffset + size <= stagingBufferSize)
    {
        memcpy(((uint8_t*)staging->GetCPUVisibleAddress() + stagingOffset), data, size);
        pendingUploads.push_back({staging.get(), &dst, size, stagingOffset, dstOffset});
        stagingOffset += size;
    }
    else
    {
        Gfx::Buffer::CreateInfo createInfo{Gfx::BufferUsage::Transfer_Src, size, true, "temp staging buffer"};
        auto buf = GetGfxDriver()->CreateBuffer(createInfo);
        memcpy(((uint8_t*)buf->GetCPUVisibleAddress() + stagingOffset), data, size);
        pendingUploads.push_back({buf.Get(), &dst, size, stagingOffset, dstOffset});
        tempBuffers.push_back(std::move(buf));
    }
}

void RenderPipeline::StagingBuffer::UploadBuffers(Gfx::CommandBuffer& cmd)
{
    for (auto& b : pendingUploads)
    {
        cmd.CopyBuffer(b.staging, b.dst, b.size, b.stagingOffset, b.dstOffset);
    }

    tempBuffers.clear();
    stagingOffset = 0;
}

RenderPipeline::StagingBuffer::StagingBuffer()
{
    Gfx::Buffer::CreateInfo createInfo{Gfx::BufferUsage::Transfer_Src, 1024 * 1024 * 32, true, "staging buffer"};
    staging = GetGfxDriver()->CreateBuffer(createInfo);
}
} // namespace Engine
