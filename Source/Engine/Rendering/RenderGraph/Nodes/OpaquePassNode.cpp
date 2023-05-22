#include "OpaquePassNode.hpp"

namespace Engine::RenderGraph
{
OpaquePassNode::OpaquePassNode(GraphContext& graphContext) : Node(graphContext)
{
    colorin = CreatePort("color in", this, ResourceType::Image, false);
    colorout = CreatePort("color out", this, ResourceType::Image, true);
}
} // namespace Engine::RenderGraph
