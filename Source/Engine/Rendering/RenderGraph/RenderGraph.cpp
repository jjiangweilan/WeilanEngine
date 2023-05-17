#include "RenderGraph.hpp"
using namespace Engine::RenderGraphImpl;

namespace Engine
{
std::unique_ptr<RenderGraph> RenderGraphBuilder::Parse(nlohmann::json& pipeline)
{
    // graph = std::unique_ptr<RenderGraph>(new RenderGraph());

    // std::vector<RenderNode> parsedNodes;
    // for (auto& jPass : pipeline)
    // {
    //     parsedNodes.push_back(ParseRenderNode(jPass));
    // }

    // return std::move(graph);
    return nullptr;
}

RenderNode RenderGraphBuilder::ParseRenderNode(nlohmann::json& renderNode)
{
    RenderNode node;
    for (auto& input : renderNode["inputs"])
    {
        node.inputs.push_back(ParseResourceHandle(input));
    }
    for (auto& output : renderNode["outputs"])
    {
        node.inputs.push_back(ParseResourceHandle(output));
    }

    node.pass = graph->CreateRenderPass(renderNode["renderPass"]);

    return node;
}

RenderGraphImpl::ResourceHandle RenderGraphBuilder::ParseResourceHandle(nlohmann::json& resourceHandle) {}

std::unique_ptr<RenderPass> RenderGraph::CreateRenderPass(const std::string& name) {}
} // namespace Engine
