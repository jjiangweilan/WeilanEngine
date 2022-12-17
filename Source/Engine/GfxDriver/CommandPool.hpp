#pragma once
#include "CommandBuffer.hpp"
#include "CommandQueue.hpp"
#include <vector>

namespace Engine::Gfx
{
    class CommandPool
    {
        public:
            struct CreateInfo
            {
                RefPtr<CommandQueue> queue;
            };
            virtual ~CommandPool() {};

            virtual std::vector<UniPtr<CommandBuffer>> AllocateCommandBuffers(CommandBufferType type, int count) = 0;
            // virtual void ReleaseCommandBuffer(RefPtr<CommandBuffer> cmdBuf);
            virtual void ResetCommandPool() = 0;
        private:
    };
}
