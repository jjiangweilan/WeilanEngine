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
    std::vector<UniPtr<Gfx::CommandBuffer>> AllocateCommandBuffers(CommandBufferType type, int count) override;
    void ResetCommandPool() override;

private:
    VkCommandPool commandPool;
    CommandQueue* queue;
};
} // namespace Engine::Gfx
