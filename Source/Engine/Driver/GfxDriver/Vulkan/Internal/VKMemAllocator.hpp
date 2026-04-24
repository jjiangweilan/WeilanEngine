#pragma once

#include "Engine/Core/Ptr.hpp"
#include "Engine/Driver/GfxDriver/Vulkan/VKCommon.hpp"
#include <functional>
#include <list>
#include <utility>
#include <vulkan/vulkan_hash.hpp>

namespace Gfx
{
struct DataRange
{
    uint32_t offsetInSrc;
    uint32_t size;
    void* data;
};

class Image;
class VKImage;
class VKBuffer;
class VKMemAllocator
{
public:
    class ScratchBuffer
    {
    public:
        enum class ScratchBufferUsage
        {
            None,
            HostVisibleScatchBuffer,
            GPUScratchBuffer
        };

    private:
        struct Block
        {
            bool inflight = false;
            int frameIndex = 0;
            int lifetime = 0;

            VkBuffer buffer = VK_NULL_HANDLE;
            VmaAllocation allocation = VK_NULL_HANDLE;

            VkDeviceAddress deviceAddress = 0;
            void* mappedData = nullptr;

            ScratchBufferUsage usage = ScratchBufferUsage::None;
            VkBufferUsageFlags bufferUsages = 0;
            uint32_t size = 0;
            uint32_t offset = 0;
        };

    public:
        struct AllocationHandle
        {
            VkBuffer buffer = VK_NULL_HANDLE;
            VkDeviceAddress deviceAddress = 0;
            void* mappedData = nullptr;

            Block* block = nullptr;
        };

        std::list<Block> blocks;
        Block* currentBlock = nullptr;
        VmaAllocator allocator_vma = VK_NULL_HANDLE;
        int currentFrameIndex = 0;

        void Init(VmaAllocator allocator);
        AllocationHandle Allocate(uint32_t size, uint32_t alignment, ScratchBufferUsage usage);

        void NewFrame(int frameIndex)
        {
            currentFrameIndex = frameIndex;
        }

        void FrameFinished(int frameIndex);
        void Destroy();

    private:
        Block& CreateBlock(uint32_t size, ScratchBufferUsage usage);
    };

    VKMemAllocator(VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice, uint32_t transferQueueIndex);
    VKMemAllocator(const VKMemAllocator& other) = delete;
    ~VKMemAllocator();

    // void UploadBuffer(RefPtr<VKBuffer> buffer, uint32_t dstOffset, size_t dataSize, DataRange data[], uint32_t
    // rangeCount); void UploadImage(RefPtr<VKImage> image, uint32_t imageSize, void* data);

    void CreateBuffer(
        VkBufferCreateInfo& createInfo,
        VmaAllocationCreateInfo& allocCreateInfo,
        VkBuffer& buffer,
        VmaAllocation& allocation,
        VmaAllocationInfo* allocationInfo = nullptr
    );
    void CreateBuffer(
        VkBufferCreateInfo& createInfo,
        VkBuffer& buffer,
        VmaAllocation& allocation,
        VmaAllocationInfo* allocationInfo = nullptr
    );
    void CreateImage(
        VkImageCreateInfo& imageCreateInfo,
        VkImage& image,
        VmaAllocation& allocation,
        VmaAllocationInfo* allocationInfo = nullptr
    );
    void DestroyBuffer(VkBuffer buffer, VmaAllocation allocation);
    void DestroyImage(VkImage image, VmaAllocation allocation);

    void DestroyPendingResources(bool destroyAll = false);

    void NewFrame(int frameIndex)
    {
        scratchBuffer.NewFrame(frameIndex);
    }

    /**
     * @brief allocate a scratch buffer for this frame, the life time of the buffer is tied to the current frame (automatically released after the frame's gpu ends)
     *
     * @param size The size of the allocation in bytes.
     * @param alignment The alignment requirement for the allocation in bytes.
     * @param usage The intended usage of the scratch buffer.
     * @return A handle to the allocated scratch buffer.
     */
    ScratchBuffer::AllocationHandle AllocateScratchBuffer(uint32_t size, uint32_t alignment, ScratchBuffer::ScratchBufferUsage usage)
    {
        auto allocation = scratchBuffer.Allocate(size, alignment, usage);
        return allocation;
    }

    void GPUFrameFinished(int frameIndex)
    {
        scratchBuffer.FrameFinished(frameIndex);
    }

    inline VmaAllocator GetHandle()
    {
        return allocator_vma;
    };

    VmaAllocator allocator_vma;
    VkDevice device;
    uint32_t queueFamilyIndex;

    VkBuffer GetStageBuffer(uint32_t size, VmaAllocation& allocation, VmaAllocationInfo& allocationInfo);

private:
    struct Info
    {
        void* ptr;
        VmaAllocation allocation;
        int frameCount;
    };

    std::list<Info> pendingBuffers;
    std::list<Info> pendingImages;
    ScratchBuffer scratchBuffer{};

    template <class T, class F>
    void DestroyPendingResourcesOfType(std::list<Info>& resources, F f, bool destroyAll);
};
} // namespace Gfx
