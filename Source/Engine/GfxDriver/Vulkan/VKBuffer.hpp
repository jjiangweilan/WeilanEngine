#pragma once
#include "../Buffer.hpp"
#include "Internal/VKMemAllocator.hpp"
#include <cinttypes>

namespace Engine::Gfx
{
class VKBuffer : public Buffer
{
public:
    VKBuffer(const CreateInfo& createInfo);
    VKBuffer(VkBufferCreateInfo& createInfo, VmaAllocationCreateInfo& allocationCreateInfo, const char* debugName);

    ~VKBuffer() override;

    void PutMemoryBarrierIfNeeded(VkCommandBuffer cmdBuf, VkPipelineStageFlags stageMask, VkAccessFlags accessMask);
    void FillMemoryBarrierIfNeeded(std::vector<VkBufferMemoryBarrier>& barriers,
                                   VkPipelineStageFlags stageMask,
                                   VkAccessFlags accessMask);
    void* GetCPUVisibleAddress() override;
    void SetDebugName(const char* name) override;
    size_t GetSize() override { return size; }

    inline VkBuffer GetHandle() { return buffer; }

private:
    std::string name;
    RefPtr<VKMemAllocator> allocator;
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = nullptr;
    size_t size;

    VkBufferUsageFlags usage = 0;
    VmaMemoryUsage vmaMemUsage = VMA_MEMORY_USAGE_AUTO;
    VmaAllocationCreateFlags vmaAllocationCreateFlags = 0;
    VmaAllocationInfo allocationInfo;
    VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkAccessFlags accessMask = VK_ACCESS_MEMORY_READ_BIT;
    void CreateBuffer();
};
} // namespace Engine::Gfx
