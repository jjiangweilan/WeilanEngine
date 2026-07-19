#pragma once
#include "Engine/Library/DynamicArray.hpp"
#include "VKRawBuffer.hpp"
#include <queue>
#include "Engine/Driver/GfxDriver/Vulkan/VKCommon.hpp"
#include "Engine/Core/Ptr.hpp"
namespace Gfx
{
class VKDriver;
class VKBuffer;
class VKImage;
class VKDataUploader
{
    struct CachedImageUpload
    {
        ObjPtr<VKImage> dst;
        std::vector<uint8_t> data;
        size_t size;
        uint32_t mipLevel;
        uint32_t arrayLayer;
        VkImageAspectFlags aspect;
        VkImageLayout finalLayout;
    };
    struct CachedBufferUpload
    {
        const VKBuffer* dst;
        std::vector<uint8_t> data;
        size_t size;
        size_t dstOffset;
    };

    std::vector<CachedImageUpload> cachedImageUploads;
    std::vector<CachedBufferUpload> cachedBufferUploads;

public:
    VKDataUploader(VKDriver* driver);
    ~VKDataUploader();

    void UploadBuffer(const VKBuffer* dst, uint8_t* data, size_t size, size_t dstOffset);
    void UploadImage(
        VKImage* dst,
        uint8_t* data,
        size_t size,
        uint32_t mipLevel,
        uint32_t arayLayer,
        VkImageAspectFlags aspect,
        VkImageLayout finalLayout
    );

    void CacheUploadImage(
        VKImage* dst,
        uint8_t* data,
        size_t size,
        uint32_t mipLevel,
        uint32_t arayLayer,
        VkImageAspectFlags aspect,
        VkImageLayout finalLayout
    )
    {
        std::vector<uint8_t> cachedData = std::vector<uint8_t>(size);
        memcpy(cachedData.data(), data, size);
        cachedImageUploads.push_back(
            CachedImageUpload(ObjPtr<VKImage>((Object*)dst), std::move(cachedData), size, mipLevel, arayLayer, aspect, finalLayout)
        );
    }

    void CacheUploadBuffer(
        const VKBuffer* dst,
        uint8_t* data,
        size_t size,
        size_t dstOffset
    )
    {
        std::vector<uint8_t> cachedData(size);
        memcpy(cachedData.data(), data, size);
        cachedBufferUploads.push_back(CachedBufferUpload{dst, std::move(cachedData), size, dstOffset});
    }

    void UploadAllPending(VkSemaphore signalSemaphore, VkSemaphore waitSemaphore, VkPipelineStageFlags waitStages);
    void WaitForUploadFinish();

private:
    struct PendingBufferUpload
    {
        VkBuffer dst;
        size_t srcOffset;
        size_t dstOffset;
        size_t size;
    };

    struct PendingImageUpload
    {
        ObjPtr<VKImage> dst;
        uint32_t width;
        uint32_t height;
        uint32_t depth;
        size_t srcOffset;
        size_t size;
        uint32_t mipLevel;
        uint32_t arrayLayer;
        VkImageAspectFlags aspect;
        VkImageLayout finalLayout;
    };

    struct InflightUploadingCmd
    {
        VkCommandBuffer cmd; // filled when dispatching
        VkFence fence = VK_NULL_HANDLE;
        size_t startOffset; // filled when newly created
        size_t endOffset;   // incremented as upload progresses
        std::vector<ObjPtr<VKImage>> imageRefs;
    };

    InflightUploadingCmd takingOffCmd = {VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0};
    std::queue<InflightUploadingCmd> inflightCmds = {};

    struct FencePool
    {
        VkFence Allocate(VkDevice device)
        {
            if (fences.empty())
            {
                VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, 0};
                VkFence fence;
                vkCreateFence(device, &fenceInfo, nullptr, &fence);
                fences.push_back(fence);
            }
            auto fence = fences.back();
            fences.pop_back();
            return fence;
        }

        void Free(VkDevice device, VkFence fence)
        {
            vkResetFences(device, 1, &fence);
            fences.push_back(fence);
        }
        std::vector<VkFence> fences;
    } fencePool;

    VKDriver* driver;
    const size_t stagingBufferSize = 1024 * 1024 * 64;
    std::vector<PendingBufferUpload> pendingBufferUploads = {};
    std::vector<PendingImageUpload> pendingImageUploads = {};
    std::vector<VkBufferCopy> copyRegions = {};
    VKRawBuffer stagingBuffer = {};

    bool EnsureEnoughSizeForUpload(InflightUploadingCmd& cmd, size_t size);
    void UploadAllPendingInternal(
        VkSemaphore signalSemaphore, VkSemaphore waitSemaphore, VkPipelineStageFlags waitStages
    );
    void FlushCachedUpload();
};
} // namespace Gfx
