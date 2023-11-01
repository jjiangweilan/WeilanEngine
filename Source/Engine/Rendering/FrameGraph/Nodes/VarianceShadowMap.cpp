#pragma once
#include "../NodeBlueprint.hpp"
#include "GfxDriver/Image.hpp"

namespace Engine::FrameGraph
{
class VarianceShadowMapNode : public Node
{
public:
    VarianceShadowMapNode(FGID id) : Node("Variance Shadow Map", id), shadowMap(true)
    {
        AddOutputProperty("shadow map", PropertyType::Image, &shadowMap);

        configs = {{"size", ConfigurableType::Vec2, glm::vec2{1024, 1024}}};
    }

    void Build(BuildResources& resources) {}

private:
    ImageProperty shadowMap;

    static char _reg;
};

char VarianceShadowMapNode::_reg = NodeBlueprintRegisteration::Register<VarianceShadowMapNode>("Variance Shadow Map");
} // namespace Engine::FrameGraph
