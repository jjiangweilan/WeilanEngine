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

            void Write(void* data, uint32_t dataSize, uint32_t offsetInDst) override;
            void* GetCPUVisibleAddress() override;
            void Resize(uint32_t size) override;

            VkBuffer GetVKBuffer();

        private:
            RefPtr<VKMemAllocator> allocator;
            VkBuffer buffer = VK_NULL_HANDLE;
            VmaAllocation allocation = nullptr;

            VkBufferUsageFlags usage = 0;
            VmaMemoryUsage vmaMemUsage = VMA_MEMORY_USAGE_AUTO;
            VmaAllocationCreateFlags vmaAllocationCreateFlags = 0;
            VmaAllocationInfo allocationInfo;
            void CreateBuffer();
    };
}
