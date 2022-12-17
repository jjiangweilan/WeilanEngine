#include "GfxResourceTransfer.hpp"
#include "GfxDriver/GfxDriver.hpp"

namespace Engine::Internal
{
    UniPtr<GfxResourceTransfer> GfxResourceTransfer::instance = nullptr;

    void GfxResourceTransfer::QueueTransferCommands(RefPtr<CommandBuffer> cmdBuf)
    {
        for(auto& [buffer, requests] : pendingBuffers)
        {
            bufferCopyRegions.clear();

            for(auto& request : requests)
            {
                bufferCopyRegions.push_back({request.srcOffset, request.userRequest.bufOffset, request.userRequest.size});
            }

            cmdBuf->CopyBuffer(stagingBuffer, buffer, bufferCopyRegions);
        }

        for(auto& [image, requests] : pendingImages)
        {
            bufferImageCopyRegions.clear();

            for(auto& request : requests)
            {
                BufferImageCopyRegion region;
                region.offset = {0,0,0};
                region.extend = { image->GetDescription().width, image->GetDescription().height, 1 };
                region.range = request.userRequest.subresourceRange;
                region.srcOffset = request.srcOffset;
            }

            cmdBuf->CopyBufferToImage(stagingBuffer, image, bufferImageCopyRegions);
        }
    }
}
