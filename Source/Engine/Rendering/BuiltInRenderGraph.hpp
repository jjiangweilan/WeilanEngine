#pragma once
#include "RenderGraph/Graph.hpp"

namespace Engine
{
class BuiltInRenderGraphBuilder
{
public:
    static std::unique_ptr<RenderGraph::Graph> BuildGraph(bool withEditor);
};
} // namespace Engine
