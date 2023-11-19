#pragma once
#include "../NodeBlueprint.hpp"
#include "Asset/Shader.hpp"

namespace Engine::FrameGraph
{
class SurfelGINode : public Node
{
    DECLARE_OBJECT();

public:
    SurfelGINode()
    {
        DefineNode();
    };
    SurfelGINode(FGID id) : Node("Surfel GI", id)
    {
        DefineNode();
    }

    std::vector<Resource> Preprocess(RenderGraph::Graph& graph) override
    {

        return {};
    }

    void Build(RenderGraph::Graph& graph, Resources& resources) override{};

    void ProcessSceneShaderResource(Gfx::ShaderResource& sceneShaderResource) override{};

    void Execute(GraphResource& graphResource) override {}

private:
    void DefineNode()
    {
        AddConfig<ConfigurableType::ObjectPtr>("GI scene", nullptr);
    }
    static char _reg;
}; // namespace Engine::FrameGraph

char SurfelGINode::_reg = NodeBlueprintRegisteration::Register<SurfelGINode>("Surfel GI");

DEFINE_OBJECT(SurfelGINode, "1810BA8A-B0B3-47CE-BF52-F1D881E98B01");
} // namespace Engine::FrameGraph
