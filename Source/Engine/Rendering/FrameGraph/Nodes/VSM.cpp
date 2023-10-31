#pragma once
#include "../NodeBlueprint.hpp"

namespace Engine::FrameGraph
{
class VSMNode : public Node
{
public:
    VSMNode(NodeID id) : Node("Variance Shadow Map", id)
    {
        AddInputProperty("float test in", PropertyType::Float, &testIn);
        AddOutputProperty("float test out", PropertyType::Float, &testOut);
    }

private:
    float testIn;
    float testOut;
    static char _reg;
};

char VSMNode::_reg = NodeBlueprintRegisteration::Register<VSMNode>("Variance Shadow Map");
} // namespace Engine::FrameGraph
