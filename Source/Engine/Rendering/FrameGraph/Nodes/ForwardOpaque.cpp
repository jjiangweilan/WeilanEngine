#pragma once
#include "../NodeBlueprint.hpp"

namespace Engine::FrameGraph
{
class ForwardOpaqueNode : public Node
{
public:
    ForwardOpaqueNode(FGID id) : Node("Forward Opaque", id), shadowMap(false), colorImage(false), depthImage(false)
    {
        AddInputProperty("color", PropertyType::Image, &colorImage);
        AddInputProperty("depth", PropertyType::Image, &depthImage);

        AddInputProperty("Shadow map", PropertyType::Image, &shadowMap);
    }

    void Build(BuildResources& resources){};

private:
    ImageProperty shadowMap;
    ImageProperty colorImage;
    ImageProperty depthImage;

    static char _reg;
};

char ForwardOpaqueNode::_reg = NodeBlueprintRegisteration::Register<ForwardOpaqueNode>("Forward Opaque");
} // namespace Engine::FrameGraph
