#pragma once
#include "Buffer.hpp"
#include <queue>
#include "Libs/DynamicArray.hpp"
#include <vulkan/vulkan.h>

namespace Gfx
{
class VKDriver;
class VKBuffer;
class VKImage;
class VKDataUploader
{
public:
    VKDataUploader(VKDriver* driver);
    ~VKDataUploader();

    void UploadBuffer(VKBuffer* dst, uint8_t* data, size_t size, size_t dstOffset);
    void UploadImage(
        VKImage* dst,
        uint8_t* data,
        size_t size,
        uint32_t mipLevel,
        uint32_t arayLayer,
        VkImageAspectFlags aspect,
        VkImageLayout finalLayout
    );
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
        VkImage dst;
        uint32_t width;
        uint32_t height;
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
        DynamicArray<VkFence> fences;
    } fencePool;

    VKDriver* driver;
    const size_t stagingBufferSize = 1024 * 1024 * 64;
    DynamicArray<PendingBufferUpload> pendingBufferUploads = {};
    DynamicArray<PendingImageUpload> pendingImageUploads = {};
    DynamicArray<VkBufferCopy> copyRegions = {};
    DynamicArray<VkImageMemoryBarrier> barriers = {};
    DynamicArray<VkBufferImageCopy> bufferImageCopies = {};
    Vulkan::Buffer stagingBuffer = {};

    bool EnsureEnoughSizeForUpload(InflightUploadingCmd& cmd, size_t size);
};
} // namespace Gfx
