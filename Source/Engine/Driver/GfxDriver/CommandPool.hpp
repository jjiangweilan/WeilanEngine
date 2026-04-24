#pragma once
#include "CommandBuffer.hpp"
#include "CommandQueue.hpp"
#include "Engine/Library/DynamicArray.hpp"

namespace Gfx
{
class CommandPool
{
public:
    struct CreateInfo
    {
        uint32_t queueFamilyIndex;
    };

    virtual ~CommandPool(){};

    virtual std::vector<std::unique_ptr<Gfx::CommandBuffer>> AllocateCommandBuffers(CommandBufferType type, int count) = 0;
    // virtual void ReleaseCommandBuffer(RefPtr<Gfx::CommandBuffer> cmdBuf);
    virtual void ResetCommandPool() = 0;

private:
};
} // namespace Gfx
