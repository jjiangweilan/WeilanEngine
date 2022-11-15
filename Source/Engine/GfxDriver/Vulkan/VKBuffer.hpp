#pragma once
#include <cinttypes>
#include "Internal/VKMemAllocator.hpp"
#include "../GfxBuffer.hpp"

namespace Engine::Gfx
{
    class VKBuffer : public GfxBuffer
    {
        public:
            VKBuffer(uint32_t size, BufferUsage usage, bool readback);
            VKBuffer(uint32_t size, VkBufferUsageFlags usage, bool readback);

            ~VKBuffer() override;

            void PutMemoryBarrierIfNeeded(VkCommandBuffer cmdBuf, VkPipelineStageFlags stageMask, VkAccessFlags accessMask);
            void Write(void* data, uint32_t dataSize, uint32_t offsetInDst) override;
            void* GetCPUVisibleAddress() override;
            void Resize(uint32_t size) override;
            void SetDebugName(const std::string& name) override;

            VkBuffer GetVKBuffer();

        private:
            RefPtr<VKMemAllocator> allocator;
            VkBuffer buffer = VK_NULL_HANDLE;
            VmaAllocation allocation = nullptr;

            VkBufferUsageFlags usage = 0;
            VmaMemoryUsage vmaMemUsage = VMA_MEMORY_USAGE_AUTO;
            VmaAllocationCreateFlags vmaAllocationCreateFlags = 0;
            VmaAllocationInfo allocationInfo;
            VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            VkAccessFlags accessMask = VK_ACCESS_2_MEMORY_READ_BIT;
            void CreateBuffer();
    };
}
