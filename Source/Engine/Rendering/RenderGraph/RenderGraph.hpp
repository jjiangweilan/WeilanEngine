#pragma once
#include "AssetDatabase/AssetDatabase.hpp"
#include "RenderPass.hpp"
#include <memory>
#include <nlohmann/json.hpp>

namespace Engine
{
namespace RenderGraphImpl
{
struct Edge
{};

struct RenderNode;

enum class ResourceHandleType
{
    Input,
    Ouput
};

struct ResourceHandle
{
    RenderNode* parent;
    ResourceHandleType type;
    std::vector<ResourceHandle*> connceted;
};

struct RenderNode
{
    std::unique_ptr<RenderPass> pass;
    std::vector<ResourceHandle> inputs;
    std::vector<ResourceHandle> outputs;
};

} // namespace RenderGraphImpl

class RenderGraph
{
public:
    RenderGraph(RefPtr<AssetDatabase> assetDatabase){};

private:
    std::unique_ptr<RenderGraphImpl::RenderPass> CreateRenderPass(const std::string& name);

    std::vector<RenderGraphImpl::RenderNode> executionNodes;

    friend class RenderGraphBuilder;
};

class RenderGraphBuilder
{
public:
    RenderGraphBuilder(RenderGraph& graph);
    std::unique_ptr<RenderGraph> Parse(nlohmann::json& pipeline);

private:
    RenderGraphImpl::RenderNode ParseRenderNode(nlohmann::json& renderNode);
    RenderGraphImpl::ResourceHandle ParseResourceHandle(nlohmann::json& resourceHandle);
    std::unique_ptr<RenderGraph> graph;
};

} // namespace Engine
