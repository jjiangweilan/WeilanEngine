#pragma once
#include "Code/Ptr.hpp"
#include "GfxDriver/GfxDriver.hpp"
namespace Engine::RGraph
{
class CommandPoolManager
{
public:
    static CommandPoolManager* GetInstance();

    UniPtr<CommandBuffer> GetCommandBuffer(int threadID);

private:
    static CommandPoolManager* GetCommandPoolManager();
    static UniPtr<CommandPoolManager> instance;
};
} // namespace Engine::RGraph

namespace Engine
{
inline RGraph::CommandPoolManager* GetCommandPoolManager() { return RGraph::CommandPoolManager::GetInstance(); }
} // namespace Engine
