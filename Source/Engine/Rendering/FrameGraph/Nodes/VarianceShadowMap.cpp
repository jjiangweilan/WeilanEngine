#pragma once
#include "../NodeBlueprint.hpp"
#include "GfxDriver/Image.hpp"

namespace Engine::FrameGraph
{
class VarianceShadowMapNode : public Node
{
    DECLARE_OBJECT();

public:
    VarianceShadowMapNode()
    {
        DefineNode();
    };
    VarianceShadowMapNode(FGID id) : Node("Variance Shadow Map", id)
    {
        DefineNode();
    }

    void Preprocess(RenderGraph::Graph& graph) override{};
    void Build(RenderGraph::Graph& graph, BuildResources& resources) override{};

private:
    void DefineNode()
    {
        AddOutputProperty("shadow map", PropertyType::Image);

        AddConfig<ConfigurableType::Vec2>("shadow map size", glm::vec2{1024, 1024});
    }
    static char _reg;
};

char VarianceShadowMapNode::_reg = NodeBlueprintRegisteration::Register<VarianceShadowMapNode>("Variance Shadow Map");
DEFINE_OBJECT(VarianceShadowMapNode, "BED9EF19-14FD-4FE9-959B-2410E44A5669");
} // namespace Engine::FrameGraph
