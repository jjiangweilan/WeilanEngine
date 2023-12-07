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
    static void Init();
    static void Deinit();

    static RenderPipeline& Singleton();

    void Schedule(const std::function<void(Gfx::CommandBuffer& cmd)>& f);
    void Render();
    void SetBufferData(Gfx::Buffer& dst, uint8_t* data, size_t size, size_t dstOffset = 0);
    void SetImageData(Gfx::Image& dst);

private:
    RenderPipeline();
    class FrameCmdBuffer
    {
    public:
        FrameCmdBuffer();
        Gfx::CommandBuffer* GetActive()
        {
            return activeCmd;
        }
        void Swap();

    private:
        Gfx::CommandBuffer* activeCmd;
        std::unique_ptr<Gfx::CommandBuffer> cmd0;
        std::unique_ptr<Gfx::CommandBuffer> cmd1;
        std::unique_ptr<Gfx::CommandPool> cmdPool;
    } frameCmdBuffer;

    struct PendingSetBuffer
    {
        Gfx::Buffer* dst;
        uint8_t* data;
        size_t size;
        size_t stagingOffset;
        size_t dstOffset;
    };

    struct StagingBuffer
    {};

    // present data
    RefPtr<CommandQueue> queue;
    std::unique_ptr<Gfx::Semaphore> submitSemaphore;
    std::unique_ptr<Gfx::Fence> submitFence;
    std::unique_ptr<Gfx::Semaphore> swapchainAcquireSemaphore;
    std::unique_ptr<Gfx::CommandPool> commandPool;
    std::unique_ptr<Gfx::Buffer> staging;

    std::vector<std::function<void(Gfx::CommandBuffer&)>> pendingWorks;
    std::vector<PendingSetBuffer> pendingSetBuffers;

    std::vector<Gfx::CommandBuffer*> cmdQueue;

    bool AcquireSwapchainImage();
    static std::unique_ptr<RenderPipeline>& SingletonPrivate();
};
}; // namespace Engine
