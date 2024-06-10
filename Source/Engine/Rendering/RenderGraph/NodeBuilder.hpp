#pragma once
#include "Errors.hpp"
#include "Graph.hpp"
#include <vector>

namespace RenderGraph
{

struct BuildResult
{
    RenderPass::ExecutionFunc execFunc;
    std::vector<RenderPass::ResourceDescription> resourceDescs;
    std::vector<RenderPass::Subpass> subpass;
};

// class NodeBuilder
// {
//
// public:
//     struct BlitDescription
//     {
//         ResourceHandle srcHandle;
//         const Gfx::ImageDescription& dstCreateInfo;
//         ResourceHandle dstHandle;
//     };
//     static BuildResult Blit(const std::vector<BlitDescription>& blits);
//
//     struct FXAADescription
//     {};
//     static BuildResult FXAA();
//
//     struct SkyboxDescription
//     {
//         ResourceHandle targetColor;
//         ResourceHandle targetDepth;
//         Gfx::Image& cubemap;
//     };
//     static RenderNode* Skybox(Graph& graph, const SkyboxDescription& desc);
// };
} // namespace RenderGraph
