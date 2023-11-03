#pragma once
#include "../NodeBlueprint.hpp"
#include "GfxDriver/Image.hpp"

namespace Engine::FrameGraph
{
class VarianceShadowMapNode : public Node
{
    DECLARE_OBJECT();

public:
    VarianceShadowMapNode(){};
    VarianceShadowMapNode(FGID id) : Node("Variance Shadow Map", id)
    {
        AddOutputProperty("shadow map", PropertyType::Image, nullptr);

        configs = {Configurable{"shadow map size", ConfigurableType::Vec2, glm::vec2{1024, 1024}}};
    }

    void Build(BuildResources& resources) override {}

private:
    static char _reg;
};

char VarianceShadowMapNode::_reg = NodeBlueprintRegisteration::Register<VarianceShadowMapNode>("Variance Shadow Map");
DEFINE_OBJECT(VarianceShadowMapNode, "BED9EF19-14FD-4FE9-959B-2410E44A5669");
} // namespace Engine::FrameGraph
