#pragma once
#include "AssetDatabase/AssetDatabase.hpp"
#include "GfxDriver/GfxEnums.hpp"
#include "GfxDriver/Image.hpp"
#include "GraphContext.hpp"
#include "Node.hpp"
#include <memory>
#include <string_view>

namespace Engine::RenderGraph
{

class Graph
{
public:
    Graph();
    ~Graph();
    // nodes should create their own resource in preprocess stage
    void Preprocess();
    // nodes can use referenced resource in another nodes in compile state
    void Compile();
    void Execute();

    template <IsNode T>
    T* AddNode();

    bool Connect(Port* src, Port* dst);

protected:
    std::unique_ptr<GraphContext> context;
    std::vector<std::unique_ptr<Node>> nodes;

    static void SetInputNodeResource(Port* src, Port* dst);
    static void UpdateConnectedNode(Port* output);
    static std::unordered_map<Node*, int> CalculateNodeDepth(std::vector<std::unique_ptr<Node>>& nodes);
};

template <IsNode T>
T* Graph::AddNode()
{
    auto node = std::make_unique<T>(*context);
    auto temp = node.get();
    nodes.push_back(std::move(node));

    return temp;
}

} // namespace Engine::RenderGraph
