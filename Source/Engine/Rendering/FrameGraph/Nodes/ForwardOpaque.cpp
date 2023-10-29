
#pragma once
#include "../NodeBlueprint.hpp"

namespace Engine::FrameGraph
{
class ForwardOpaqueNode : public Node
{
public:
    ForwardOpaqueNode(NodeID id) : Node("Forward Opaque", id) {}

private:
    static char _reg;
};

char ForwardOpaqueNode::_reg = NodeBlueprintRegisteration::Register<ForwardOpaqueNode>("Forward Opaque");
} // namespace Engine::FrameGraph
