#include "GfxResourceTransfer.hpp"
#include "GfxDriver/GfxDriver.hpp"

namespace Engine::Internal
{
    UniPtr<GfxResourceTransfer> GfxResourceTransfer::instance = nullptr;

    void GfxResourceTransfer::QueueTransferCommands(RefPtr<CommandBuffer> cmdBuf)
    {
        std::vector<MemoryBarrier> memoryBarrier;

        for(auto buffer : pending.buffers)
        {
            MemoryBarrier memBarrier;
            memBarrier.buffer = buffer.val;
            memBarrier.bufferInfo.dstQueueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED;
            memBarrier.bufferInfo.offset = 0;
            memBarrier.bufferInfo.size = GFX_WHOLE_SIZE;
            memBarrier.dstStageMask = buffer.request.dstStageMasks;
            memBarrier.dstAccessMask
        }
    }
}
