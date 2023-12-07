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

    staging.Upload(*cmd);

    Gfx::GPUBarrier barrier{
        .srcStageMask = Gfx::PipelineStage::Transfer,
        .dstStageMask = Gfx::PipelineStage::Top_Of_Pipe,
        .srcAccessMask = Gfx::AccessMask::Memory_Read | Gfx::AccessMask::Memory_Write,
        .dstAccessMask = Gfx::AccessMask::Memory_Read | Gfx::AccessMask::Memory_Write,
    };
    cmd->Barrier(&barrier, 1);

    for (auto& f : pendingWorks)
    {
        f(*cmd);
    }
    cmd->End();

    cmdQueue.clear();
    cmdQueue.push_back(cmd);

    GetGfxDriver()->WaitForFence({submitFence}, true, -1);
    submitFence->Reset();

    GetGfxDriver()->ClearResources();
    staging.Clear(); // staing resources will be deleted next frame

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
        memcpy(((uint8_t*)buf->GetCPUVisibleAddress()), data, size);
        pendingUploads.push_back({buf.Get(), &dst, size, 0, dstOffset});
        tempBuffers.push_back(std::move(buf));
    }
}

void RenderPipeline::StagingBuffer::Upload(Gfx::CommandBuffer& cmd)
{
    for (auto& b : pendingUploads)
    {
        cmd.CopyBuffer(b.staging, b.dst, b.size, b.stagingOffset, b.dstOffset);
    }

    for (auto& i : pendingImages)
    {
        Gfx::GPUBarrier barrier{
            .image = i.dst,
            .srcStageMask = Gfx::PipelineStage::Top_Of_Pipe,
            .dstStageMask = Gfx::PipelineStage::Transfer,
            .srcAccessMask = Gfx::AccessMask::None,
            .dstAccessMask = Gfx::AccessMask::Transfer_Write,

            .imageInfo = {
                .srcQueueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED,
                .oldLayout = Gfx::ImageLayout::Undefined,
                .newLayout = Gfx::ImageLayout::Transfer_Dst,
                .subresourceRange =
                    {
                        .aspectMask = i.dst->GetSubresourceRange().aspectMask,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                    },
            }};

        auto& desc = i.dst->GetDescription();
        Gfx::BufferImageCopyRegion c[1] = {{
            .srcOffset = i.stagingOffset,
            .layers =
                {.aspectMask = i.dst->GetSubresourceRange().aspectMask,
                 .mipLevel = i.mipLevel,
                 .baseArrayLayer = i.arrayLayer,
                 .layerCount = 1},
            .offset = {0, 0, 0},
            .extend = {desc.width, desc.height, 1},

        }};

        cmd.Barrier(&barrier, 1);
        cmd.CopyBufferToImage(i.staging, i.dst, c);

        barrier.imageInfo.oldLayout = Gfx::ImageLayout::Transfer_Dst;
        barrier.imageInfo.newLayout = Gfx::ImageLayout::Shader_Read_Only;
        barrier.srcStageMask = Gfx::PipelineStage::Transfer;
        barrier.dstStageMask = Gfx::PipelineStage::Top_Of_Pipe;
        barrier.srcAccessMask = Gfx::AccessMask::Transfer_Write;
        barrier.dstAccessMask = Gfx::AccessMask::None;
        cmd.Barrier(&barrier, 1);
    }

    pendingImages.clear();
    pendingUploads.clear();
    stagingOffset = 0;
}

void RenderPipeline::StagingBuffer::Clear()
{
    tempBuffers.clear();
}

void RenderPipeline::UploadImage(Gfx::Image& dst, uint8_t* data, size_t size, uint32_t mipLevel, uint32_t arayLayer)
{
    staging.UploadImage(dst, data, size, mipLevel, arayLayer);
}

void RenderPipeline::StagingBuffer::UploadImage(
    Gfx::Image& dst, uint8_t* data, size_t size, uint32_t mipLevel, uint32_t arayLayer
)
{
    const size_t stagingBufferSize = staging->GetSize();

    if (stagingOffset + size <= stagingBufferSize)
    {
        memcpy(((uint8_t*)staging->GetCPUVisibleAddress() + stagingOffset), data, size);
        pendingImages.push_back({staging.get(), &dst, stagingOffset, size, mipLevel, arayLayer});
        stagingOffset += size;
    }
    else
    {
        Gfx::Buffer::CreateInfo createInfo{Gfx::BufferUsage::Transfer_Src, size, true, "temp staging buffer"};
        std::unique_ptr<Gfx::Buffer> buf = GetGfxDriver()->CreateBuffer(createInfo);
        memcpy(((uint8_t*)buf->GetCPUVisibleAddress()), data, size);
        pendingImages.push_back({buf.get(), &dst, 0, size, mipLevel, arayLayer});
        tempBuffers.push_back(std::move(buf));
    }
}

RenderPipeline::StagingBuffer::StagingBuffer()
{
    Gfx::Buffer::CreateInfo createInfo{Gfx::BufferUsage::Transfer_Src, 1024 * 1024 * 32, true, "staging buffer"};
    staging = GetGfxDriver()->CreateBuffer(createInfo);
}
} // namespace Engine
