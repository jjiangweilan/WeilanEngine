#pragma once
#include "../Node.hpp"
#include "GfxDriver/GfxDriver.hpp"

namespace Engine::RenderGraph
{
class OpaquePassNode : public Node
{
public:
    OpaquePassNode(GraphContext& graphContext);

    Port* GetPortColorIn() { return colorin.get(); }
    Port* GetPortColorOut() { return colorout.get(); }

private:
    std::unique_ptr<Port> colorin;
    std::unique_ptr<Port> colorout;
};
} // namespace Engine::RenderGraph
