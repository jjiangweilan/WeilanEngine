#pragma once
#include "Graph.hpp"
#include "Rendering/RenderGraph/Nodes/ImageNode.hpp"
#include <nlohmann/json.hpp>

namespace Engine::RenderGraph
{
class JsonGraphBuilder
{
public:
    JsonGraphBuilder(nlohmann::json& j) : j(j) {}
    std::unique_ptr<Graph> Build();

private:
    std::unique_ptr<Graph> graph;
    std::unordered_map<std::string, ImageNode*> imageNodes;
    nlohmann::json j;
};
} // namespace Engine::RenderGraph
