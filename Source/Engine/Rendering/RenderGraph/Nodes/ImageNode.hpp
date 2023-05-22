#pragma once

#include "../Node.hpp"
#include <glm/glm.hpp>
namespace Engine::RenderGraph
{
class ImageNode : public Node
{
public:
    ImageNode(GraphContext& graphContext);

    Port* GetPortImage() { return outputPort.get(); }

    Gfx::ImageFormat format;
    glm::uvec3 size;

private:
    std::unique_ptr<Port> outputPort;
    ResourceRef imageHandle;
};

} // namespace Engine::RenderGraph
