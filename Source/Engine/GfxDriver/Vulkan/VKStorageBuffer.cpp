#include "VKStorageBuffer.hpp"
#include "GfxDriver/Vulkan/VKContext.hpp"

namespace Engine::Gfx
{
    VKStorageBuffer::VKStorageBuffer(int size) : size(size)
    {
        buffer = MakeUnique<VKBuffer>(size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, false);
    }

    void VKStorageBuffer::UpdateData(void* data)
    {
        buffer->Write(data, size, 0);
    }
}
