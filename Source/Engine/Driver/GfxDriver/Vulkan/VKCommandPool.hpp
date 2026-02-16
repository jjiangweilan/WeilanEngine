#pragma once
#include "../CommandPool.hpp"
#include "VKCommandBuffer.hpp"
#include "Engine/Driver/GfxDriver/Vulkan/VKCommon.hpp"

namespace Gfx
{
class VKCommandPool : public CommandPool
{
public:
    VKCommandPool(const CreateInfo& createInfo);
    ~VKCommandPool() override;
    std::vector<std::unique_ptr<Gfx::CommandBuffer>> AllocateCommandBuffers(CommandBufferType type, int count) override;
    void ResetCommandPool() override;

private:
    VkCommandPool commandPool;
    CommandQueue* queue;
};
} // namespace Gfx
