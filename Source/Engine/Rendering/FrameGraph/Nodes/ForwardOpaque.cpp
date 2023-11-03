#pragma once
#include "../NodeBlueprint.hpp"

namespace Engine::FrameGraph
{
class ForwardOpaqueNode : public Node
{
    DECLARE_OBJECT();

public:
    ForwardOpaqueNode(){};
    ForwardOpaqueNode(FGID id) : Node("Forward Opaque", id)
    {
        AddInputProperty("color", PropertyType::Image, nullptr);
        AddInputProperty("depth", PropertyType::Image, nullptr);

        AddInputProperty("Shadow map", PropertyType::Image, nullptr);
    }

    void Build(BuildResources& resources) override{};

private:
    static char _reg;
};

char ForwardOpaqueNode::_reg = NodeBlueprintRegisteration::Register<ForwardOpaqueNode>("Forward Opaque");

DEFINE_OBJECT(ForwardOpaqueNode, "E6188926-D83E-4B17-9C7C-060A5862BDCA");
} // namespace Engine::FrameGraph
