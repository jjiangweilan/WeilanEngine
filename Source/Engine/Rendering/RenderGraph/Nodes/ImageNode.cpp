#include "ImageNode.hpp"

namespace Engine::RenderGraph
{
ImageNode::ImageNode(GraphContext& graphContext) : Node(graphContext)
{
    outputPort = CreatePort("image", this, ResourceType::Image, true);
}
} // namespace Engine::RenderGraph
