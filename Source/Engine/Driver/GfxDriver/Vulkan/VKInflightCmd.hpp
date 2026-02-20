#pragma once
#include "Engine/Driver/GfxDriver/Vulkan/VKCommandBuffer.hpp"
#include "Engine/Driver/GfxDriver/Vulkan/VKCommon.hpp"

namespace Gfx
{
struct VKFrameContext
{
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkFence cmdFence = VK_NULL_HANDLE;
    uint32_t swapchainIndex = 0;

    VkQueryPool timestapQueryPool = VK_NULL_HANDLE;
    int maxtimestapQueryCount = 0;
    size_t frameIndex = 0;

    std::vector<std::function<void()>> onCompleteCallbacks{};
};

struct VKFramePrepareData
{
    void AppendVKCommandBuffer(VKCommandBuffer* cmd);
    void Clear();

    std::vector<VKCmd> cmds{};
    std::list<std::shared_ptr<AsyncReadbackHandle>> readbacks{};
};
} // namespace Gfx
