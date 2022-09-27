#pragma once
#include <cinttypes>
#include "../Internal/VKMemAllocator.hpp"
#include "../../Buffer.hpp"

namespace Engine::Gfx
{
    struct VKContext;
}

namespace Engine::Gfx::Exp
{
    class VKBuffer : public Buffer
    {
        public:
            VKBuffer(RefPtr<VKContext> context, uint32_t size, BufferUsage usage, bool readback);
            VKBuffer(RefPtr<VKContext> context, uint32_t size, VkBufferUsageFlags usage, bool readback);

            ~VKBuffer() override;

            void Write(void* data, uint32_t dataSize, uint32_t offsetInDst) override;
            void* GetCPUVisibleAddress() override;
            void Resize(uint32_t size) override;

            VkBuffer GetVKBuffer();

        private:
            VkBuffer buffer = VK_NULL_HANDLE;
            VmaAllocation allocation = nullptr;

            RefPtr<VKContext> context;

            VkBufferUsageFlags usage = 0;
            VmaMemoryUsage vmaMemUsage = VMA_MEMORY_USAGE_AUTO;
            VmaAllocationCreateFlags vmaAllocationCreateFlags = 0;
            VmaAllocationInfo allocationInfo;
            void CreateBuffer();
    };
}