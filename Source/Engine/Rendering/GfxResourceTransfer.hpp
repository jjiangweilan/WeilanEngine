#pragma once
#include "Code/Ptr.hpp"
#include "GfxDriver/Buffer.hpp"
#include "GfxDriver/CommandBuffer.hpp"
#include "GfxDriver/GfxEnums.hpp"
#include "GfxDriver/Image.hpp"
#include <functional>
#include <vector>

namespace Engine::Internal
{
class GfxResourceTransfer
{
public:
    struct BufferTransferRequest
    {
        void* data;
        uint64_t bufOffset;
        uint64_t size;
    };

    struct ImageTransferRequest
    {
        void* data;
        uint64_t size;
        Gfx::ImageSubresourceRange subresourceRange;
    };

    void Transfer(RefPtr<Gfx::Buffer> buffer, const BufferTransferRequest& request)
    {
        memcpy((char*)stagingBuffer->GetCPUVisibleAddress() + stagingBufferOffset, request.data, request.size);
        pendingBuffers[buffer.Get()].push_back({buffer, stagingBufferOffset, request});
        stagingBufferOffset += request.size;
    }

    void Transfer(RefPtr<Gfx::Image> image, const ImageTransferRequest& request)
    {
        memcpy((char*)stagingBuffer->GetCPUVisibleAddress() + stagingBufferOffset, request.data, request.size);
        pendingImages[image.Get()].push_back({image, stagingBufferOffset, request});
        stagingBufferOffset += request.size;
    }

    // method assumes all the memory are not in use
    void QueueTransferCommands(RefPtr<CommandBuffer> cmdBuf);
    static void DestroyGfxResourceTransfer() { instance = nullptr; };

private:
    GfxResourceTransfer();
    inline static RefPtr<GfxResourceTransfer> Instance()
    {
        if (instance == nullptr)
        {
            instance = UniPtr<GfxResourceTransfer>(new GfxResourceTransfer());
        }

        return instance;
    }

    struct BufferTransferRequestInternalUse
    {
        RefPtr<Gfx::Buffer> buf;
        uint64_t srcOffset;
        BufferTransferRequest userRequest;
    };

    struct ImageTransferRequestInternalUse
    {
        RefPtr<Gfx::Image> image;
        uint64_t srcOffset;
        ImageTransferRequest userRequest;
    };

    static UniPtr<GfxResourceTransfer> instance;

    std::unordered_map<Gfx::Buffer*, std::vector<BufferTransferRequestInternalUse>> pendingBuffers;
    std::unordered_map<Gfx::Image*, std::vector<ImageTransferRequestInternalUse>> pendingImages;
    std::vector<BufferCopyRegion> bufferCopyRegions;
    std::vector<BufferImageCopyRegion> bufferImageCopyRegions;
    UniPtr<Gfx::Buffer> stagingBuffer;
    uint32_t stagingBufferOffset = 0;

    friend RefPtr<GfxResourceTransfer> GetGfxResourceTransfer();
};

inline RefPtr<GfxResourceTransfer> GetGfxResourceTransfer() { return GfxResourceTransfer::Instance(); }
} // namespace Engine::Internal
