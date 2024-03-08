#pragma once
#include "../NodeBlueprint.hpp"
#include "Asset/Shader.hpp"
#include "Rendering/SurfelGI/GIScene.hpp"

namespace FrameGraph
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

    // void Execute(RenderingData& renderingData) override {}

private:
    RenderGraph::RenderNode* node;

    std::unique_ptr<Gfx::Buffer> buf;
    std::unique_ptr<Gfx::ShaderResource> passResource;
    Gfx::ShaderProgram* program;
    void DefineNode()
    {
        AddInputProperty("color", PropertyType::RenderGraphLink);
        AddInputProperty("depth", PropertyType::RenderGraphLink);

        AddOutputProperty("color", PropertyType::RenderGraphLink);
        AddOutputProperty("depth", PropertyType::RenderGraphLink);
        AddConfig<ConfigurableType::ObjectPtr>("GI Scene", nullptr);
        AddConfig<ConfigurableType::Bool>("Debug", false);
        AddConfig<ConfigurableType::ObjectPtr>("Surfel Cube Shader", nullptr);
        AddConfig<ConfigurableType::ObjectPtr>("Surfel Debug Arrow", nullptr);
    }
    static char _reg;
}; // namespace FrameGraph

char SurfelGINode::_reg = NodeBlueprintRegisteration::Register<SurfelGINode>("Surfel GI");

DEFINE_OBJECT(SurfelGINode, "1810BA8A-B0B3-47CE-BF52-F1D881E98B01");
} // namespace FrameGraph
