#pragma once
#include "../CommandPool.hpp"
#include "VKCommandBuffer.hpp"
#include <vulkan/vulkan.h>

namespace Engine::Gfx
{
    class VKCommandPool : public CommandPool
    {
        public:
            VKCommandPool(const CreateInfo& createInfo);
            ~VKCommandPool() override;
            std::vector<UniPtr<CommandBuffer>> AllocateCommandBuffers(CommandBufferType type, int count) override;
            void ReleaseCommandBuffer(RefPtr<CommandBuffer> cmdBuf) override; // we reset the whole command pool per frame, so this is not used

            void ResetCommandPool();
        private:
            VkCommandPool commandPool;
    };
}
