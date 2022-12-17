#pragma once
#include "Code/Ptr.hpp"
#include "GfxDriver/CommandBuffer.hpp"
#include "GfxDriver/Buffer.hpp"
#include "GfxDriver/Image.hpp"
#include "GfxDriver/GfxEnums.hpp"
#include <vector>
#include <functional>

namespace Engine::Internal
{
    class GfxResourceTransfer
    {
        public:
            struct TransferRequest
            {
                void* data;
                uint32_t size;
                std::function<void(void* data, uint32_t size)> onTransferFinished;
                Gfx::ImageLayout imageLayoutAfterTransfer;
                Gfx::PipelineStageFlags dstStageMasks;
            };

            void Transfer(RefPtr<Gfx::Buffer> buffer, const TransferRequest& request)
            {
                pending.buffers.push_back({PendingType::Buffer, buffer, request});
            }

            void Transfer(RefPtr<Gfx::Image> image, const TransferRequest& request)
            {
                pending.images.push_back({PendingType::Image, image ,request});
            }

            void QueueTransferCommands(RefPtr<CommandBuffer> cmdBuf);

        private:
            inline static RefPtr<GfxResourceTransfer> Instance()
            {
                if (instance == nullptr)
                {
                    instance = MakeUnique<GfxResourceTransfer>();
                }

                return instance;
            }

            enum class PendingType
            {
                Image, Buffer
            };

            template<class T>
            struct Pending
            {
                PendingType isImage;
                RefPtr<T> val;
                TransferRequest request;
            };

            struct
            {
                std::vector<Pending<Gfx::Buffer>> buffers;
                std::vector<Pending<Gfx::Image>> images;
            } pending;

            struct
            {
                UniPtr<Gfx::Buffer> stageBuffer;
            } stageBufferCache;

        private:
            GfxResourceTransfer();
            static UniPtr<GfxResourceTransfer> instance;

            friend RefPtr<GfxResourceTransfer> GetGfxResourceTransfer();

    };

    inline RefPtr<GfxResourceTransfer> GetGfxResourceTransfer()
    {
        return GfxResourceTransfer::Instance();
    }
}
