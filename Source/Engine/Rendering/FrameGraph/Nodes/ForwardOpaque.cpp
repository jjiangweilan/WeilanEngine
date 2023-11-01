#pragma once
#include "../NodeBlueprint.hpp"

namespace Engine::FrameGraph
{
class ForwardOpaqueNode : public Node
{
public:
    ForwardOpaqueNode(FGID id) : Node("Forward Opaque", id)
    {
        AddInputProperty("float test in", PropertyType::Float, &testIn);
        AddOutputProperty("float test out", PropertyType::Float, &testOut);
    }

    void Build(BuildResources& resources){};

private:
    float testIn;
    float testOut;
    static char _reg;
};

char ForwardOpaqueNode::_reg = NodeBlueprintRegisteration::Register<ForwardOpaqueNode>("Forward Opaque");
} // namespace Engine::FrameGraph
