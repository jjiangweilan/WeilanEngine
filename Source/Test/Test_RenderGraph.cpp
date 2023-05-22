#include "AssetDatabase/Asset.hpp"
#include "Rendering/RenderGraph/Graph.hpp"
#include "Rendering/RenderGraph/JsonGraphBuilder.hpp"
#include "Rendering/RenderGraph/Nodes/ImageNode.hpp"
#include "Rendering/RenderGraph/Nodes/OpaquePassNode.hpp"
#include <gtest/gtest.h>

using namespace Engine;
using namespace Engine::RenderGraph;

TEST(RenderGraph, JsonBuilderAndInsert) {}

namespace Engine::RenderGraph
{
class RenderGraphUnitTest : public Graph
{
public:
    static void TestGraph()
    {
        RenderGraphUnitTest graph;
        RenderGraph::ImageNode* imageNode = graph.AddNode<RenderGraph::ImageNode>();
        RenderGraph::OpaquePassNode* opaquePass = graph.AddNode<RenderGraph::OpaquePassNode>();

        graph.Connect(imageNode->GetPortImage(), opaquePass->GetPortColorIn());
        auto depth = graph.CalculateNodeDepth(graph.nodes);

        EXPECT_EQ(depth[imageNode], 1);
        EXPECT_EQ(depth[opaquePass], 2);
    }
};
} // namespace Engine::RenderGraph
TEST(RenderGraph, Graph) { RenderGraphUnitTest::TestGraph(); }
