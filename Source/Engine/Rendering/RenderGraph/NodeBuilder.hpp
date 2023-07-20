#pragma once
#include "Graph.hpp"
#include <vector>

namespace Engine::RenderGraph
{
struct BuildResult
{
    RenderPass::ExecutionFunc execFunc;
    std::vector<RenderPass::ResourceDescription> resourceDescs;
    std::vector<RenderPass::Subpass> subpass;
};

class NodeBuilder
{

public:
    struct BlitDescription
    {
        ResourceHandle srcHandle;
        const Gfx::ImageDescription& dstCreateInfo;
        ResourceHandle dstHandle;
    };

    static BuildResult Blit(const std::vector<BlitDescription>& blits);
};
} // namespace Engine::RenderGraph
