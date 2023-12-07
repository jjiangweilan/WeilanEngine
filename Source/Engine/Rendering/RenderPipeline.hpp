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

    void Schedule(std::function<void(Gfx::CommandBuffer& cmd)>&& f);
    void Render();
    void UploadBuffer(Gfx::Buffer& dst, uint8_t* data, size_t size, size_t dstOffset = 0);
    void UploadImage(Gfx::Image& dst, uint8_t* data, size_t size, uint32_t mipLevel = 0, uint32_t arayLayer = 0);

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

    struct PendingBufferUpload
    {
        Gfx::Buffer* staging;
        Gfx::Buffer* dst;
        size_t size;
        size_t stagingOffset;
        size_t dstOffset;
    };

    struct PendingImageUpload
    {
        Gfx::Buffer* staging;
        Gfx::Image* dst;
        size_t stagingOffset;
        size_t size;
        uint32_t mipLevel;
        uint32_t arrayLayer;
    };

    class StagingBuffer
    {
    public:
        void Upload(Gfx::CommandBuffer& cmd);
        void UploadBuffer(Gfx::Buffer& dst, uint8_t* data, size_t size, size_t dstOffset);
        void UploadImage(Gfx::Image& dst, uint8_t* data, size_t size, uint32_t mipLevel, uint32_t arrayLayer);
        void Clear();

        StagingBuffer();

    private:
        std::unique_ptr<Gfx::Buffer> staging;
        // when staging is used up, temp staging buffers will be created.
        std::vector<std::unique_ptr<Gfx::Buffer>> tempBuffers;
        std::vector<PendingBufferUpload> pendingUploads;
        std::vector<PendingImageUpload> pendingImages;
        size_t stagingOffset = 0;
    } staging;

    // present data
    RefPtr<CommandQueue> queue;
    std::unique_ptr<Gfx::Semaphore> submitSemaphore;
    std::unique_ptr<Gfx::Fence> submitFence;
    std::unique_ptr<Gfx::Semaphore> swapchainAcquireSemaphore;
    std::unique_ptr<Gfx::CommandPool> commandPool;

    std::vector<std::function<void(Gfx::CommandBuffer&)>> pendingWorks;
    std::vector<PendingBufferUpload> pendingSetBuffers;

    std::vector<Gfx::CommandBuffer*> cmdQueue;

    bool AcquireSwapchainImage();
    static std::unique_ptr<RenderPipeline>& SingletonPrivate();
};
}; // namespace Engine
