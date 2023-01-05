#pragma once
#include "Libs/Ptr.hpp"
#include "GfxDriver/Buffer.hpp"
#include "GfxDriver/CommandBuffer.hpp"
#include "GfxDriver/CommandPool.hpp"
#include "GfxDriver/Fence.hpp"
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
        bool keepData = false;
        Gfx::ImageSubresourceRange subresourceRange;
    };

    void Transfer(Gfx::ShaderResource* shaderResource, const std::string& param, const std::string& member, void* value)
    {
        Gfx::ShaderResource::BufferMemberInfoMap memberInfo;
        auto buffer = shaderResource->GetBuffer(param, memberInfo);
        auto memberInfoIter = memberInfo.find(member);
        if (memberInfoIter == memberInfo.end()) return;

        Internal::GfxResourceTransfer::BufferTransferRequest request{
            .data = value,
            .bufOffset = memberInfoIter->second.offset,
            .size = memberInfoIter->second.size,
        };
        Transfer(buffer, request);
    }

    void Transfer(RefPtr<Gfx::Buffer> buffer, const BufferTransferRequest& request);

    void Transfer(RefPtr<Gfx::Image> image, const ImageTransferRequest& request);

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

    // used when staging buffer is full and we call this method to upload all the data to gpu
    void ImmediateSubmit();

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
    const uint32_t stagingBufferSize = 1024 * 1024 * 30;
    UniPtr<Gfx::CommandPool> cmdPool;
    UniPtr<CommandBuffer> cmdBuf;
    UniPtr<Gfx::Fence> fence;

    friend RefPtr<GfxResourceTransfer> GetGfxResourceTransfer();
};

inline RefPtr<GfxResourceTransfer> GetGfxResourceTransfer() { return GfxResourceTransfer::Instance(); }
} // namespace Engine::Internal
