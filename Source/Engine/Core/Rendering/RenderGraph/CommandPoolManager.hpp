#pragma once
#include "GfxDriver/GfxDriver.hpp"
#include "Libs/Ptr.hpp"
namespace Engine::RGraph
{
class CommandPoolManager
{
public:
    static CommandPoolManager* GetInstance();

    UniPtr<Gfx::CommandBuffer> GetCommandBuffer(int threadID);

private:
    std::unordered_map<int, UniPtr<Gfx::CommandPool>> pools;
    static UniPtr<CommandPoolManager> instance;
};
} // namespace Engine::RGraph

namespace Engine
{
inline RGraph::CommandPoolManager* GetCommandPoolManager() { return RGraph::CommandPoolManager::GetInstance(); }
} // namespace Engine
