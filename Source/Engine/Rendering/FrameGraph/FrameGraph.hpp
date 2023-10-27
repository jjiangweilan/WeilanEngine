#pragma once
#include "Rendering/RenderGraph/Graph.hpp"
#include <nlohmann/json.hpp>
#include <span>

namespace Engine::FrameGraph
{

class Node
{
public:
    virtual void GetInput();
    virtual void GetOutput();
};

class Graph
{
public:
    void BuildGraph(Graph& graph, nlohmann::json& j);
    void Execute(Graph& graph);
    std::span<std::unique_ptr<Node>> GetNodes()
    {
        return nodes;
    }

    std::vector<std::unique_ptr<Node>> nodes;

private:
    using RGraph = RenderGraph::Graph;

    RGraph* targetGraph;
};
} // namespace Engine::FrameGraph
