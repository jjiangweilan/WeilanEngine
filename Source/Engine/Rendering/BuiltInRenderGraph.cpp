#include "BuiltInRenderGraph.hpp"
// #include "RenderGraph/Nodes/ExternalImageNode.hpp"
// #include "RenderGraph/Nodes/ImageNode.hpp"
// #include "RenderGraph/Nodes/OpaquePassNode.hpp"

// #if GAME_EDITOR
// #include "Editor/Rendering/EditorPass.hpp"
// #endif

namespace Engine
{
using namespace RenderGraph;
std::unique_ptr<RenderGraph::Graph> BuiltInRenderGraphBuilder::BuildGraph(bool withEditor)
{
    auto graph = std::make_unique<Graph>();
//     auto swapchainImage = graph->AddNode<ExternalImageNode>();
//     auto depthImage = graph->AddNode<ImageNode>();
//     auto opaquepass = graph->AddNode<OpaquePassNode>();

//     graph->Connect(swapchainImage->GetPortImage(), opaquepass->GetPortColorIn());
//     graph->Connect(depthImage->GetPortImage(), opaquepass->GetPortDepthIn());

// #if GAME_EDITOR
//     if (withEditor)
//     {
//         auto editorPass = graph->AddNode<Editor::EditorPass>();
//     }
// #endif

    return graph;
}
} // namespace Engine
