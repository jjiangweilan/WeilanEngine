#pragma once
#include "RenderGraph/RenderGraph.hpp"

namespace Engine::Rendering
{
class Server
{
public:
    void ExecuteGraph(RenderGraph& renderGraph);
    void Present();
};

Server* GetServer();
} // namespace Engine::Rendering
