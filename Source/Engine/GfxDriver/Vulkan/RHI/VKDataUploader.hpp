#pragma once
#include "Buffer.hpp"
#include <vector>
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
        VKImage* dst, uint8_t* data, size_t size, uint32_t mipLevel, uint32_t arayLayer, VkImageAspectFlags aspect
    );
    void UploadAllPending(VkSemaphore signalSemaphore);

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
    };

    VKDriver* driver;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkFence fence;
    const size_t stagingBufferSize = 1024 * 1024 * 48;
    size_t offset = 0;
    std::vector<PendingBufferUpload> pendingBufferUploads = {};
    std::vector<PendingImageUpload> pendingImageUploads = {};
    std::vector<VkBufferCopy> copyRegions = {};
    std::vector<VkImageMemoryBarrier> barriers = {};
    std::vector<VkBufferImageCopy> bufferImageCopies = {};
    Vulkan::Buffer stagingBuffer = {};
};
} // namespace Gfx
