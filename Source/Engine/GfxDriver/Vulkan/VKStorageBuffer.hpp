#pragma once
#include "../StorageBuffer.hpp"
#include "VKBuffer.hpp"

namespace Engine::Gfx
{
    class VKStorageBuffer : public StorageBuffer
    {
        public:
            VKStorageBuffer(int size);
            void UpdateData(void* data) override;
            VkBuffer GetVKBuffer() { return buffer->GetVKBuffer(); }
            uint32_t GetSize() override {return size;}
            void PutMemoryBarrierIfNeeded(VkCommandBuffer cmdBuf, VkPipelineStageFlags stageMask, VkAccessFlags accessMask);

        private:

            UniPtr<VKBuffer> buffer;
            uint32_t size;
    };
}
