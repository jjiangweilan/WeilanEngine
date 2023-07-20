#include "CommandPoolManager.hpp"

namespace Engine::RGraph
{
CommandPoolManager* CommandPoolManager::GetInstance()
{
    if (instance == nullptr)
    {
        instance = MakeUnique<CommandPoolManager>();
    }
    return instance.Get();
}

UniPtr<Gfx::CommandBuffer> CommandPoolManager::GetCommandBuffer(int threadID)
{
    auto iter = pools.find(threadID);
    if (iter == pools.end())
    {
        Gfx::CommandPool::CreateInfo createInfo;
        createInfo.queueFamilyIndex = GetGfxDriver()->GetQueue(QueueType::Main)->GetFamilyIndex();
        pools.emplace(threadID, GetGfxDriver()->CreateCommandPool(createInfo));
    }

    return std::move(pools[threadID]->AllocateCommandBuffers(Gfx::CommandBufferType::Primary, 1)[0]);
}

UniPtr<CommandPoolManager> CommandPoolManager::instance = nullptr;
} // namespace Engine::RGraph
