#include "VKContext.hpp"
#include <functional>
namespace Engine::Gfx
{
    void VKContext::AppendFramePrepareCommands(std::function<void(VkCommandBuffer)>&& cmdBuf)
    {
        pendingPrepareCommands.push_back(std::move(cmdBuf));
    }

    void VKContext::RecordFramePrepareCommands(VkCommandBuffer cmdBuf)
    {
        for(auto& p : pendingPrepareCommands)
        {
            p(cmdBuf);
        }
        pendingPrepareCommands.clear();
    }
}