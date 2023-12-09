#pragma once
#include "../NodeBlueprint.hpp"
#include "Asset/Shader.hpp"

namespace Engine::FrameGraph
{
class FullScreenPassNode : public Node
{
    DECLARE_OBJECT();

public:
    FullScreenPassNode()
    {
        DefineNode();
    };
    FullScreenPassNode(FGID id) : Node("Full Screen Pass", id)
    {
        DefineNode();
    }

    std::vector<Resource> Preprocess(RenderGraph::Graph& graph) override
    {
        Shader* shader = GetConfigurableVal<Shader*>("shader");
        passResource =
            GetGfxDriver()->CreateShaderResource(shader->GetDefaultShaderProgram(), Gfx::ShaderResourceFrequency::Pass);

        ClearConfigs();
        AddConfig<ConfigurableType::ObjectPtr>("shader", shader);

        for (auto& b : shader->GetDefaultShaderProgram()->GetShaderInfo().bindings)
        {
            if (b.second.type == Gfx::ShaderInfo::BindingType::UBO)
            {
                for (auto& m : b.second.binding.ubo.data.members)
                {
                    if (m.second.data->type == Gfx::ShaderInfo::ShaderDataType::Float)
                    {
                        AddConfig<ConfigurableType::Float>(m.first.data(), 0.0f);
                    }
                }
            }
        }

        Gfx::ClearValue v;
        v.color.float32[0] = 0;
        v.color.float32[1] = 0;
        v.color.float32[2] = 0;
        v.color.float32[3] = 0;

        std::vector<Gfx::ClearValue> clears = {v};
        auto execFunc =
            [this, clears, shader](Gfx::CommandBuffer& cmd, Gfx::RenderPass& pass, const RenderGraph::ResourceRefs& res)
        {
            if (shader != nullptr)
            {
                cmd.BeginRenderPass(pass, clears);
                cmd.BindShaderProgram(shader->GetDefaultShaderProgram(), shader->GetDefaultShaderConfig());
                cmd.BindResource(passResource);
                cmd.Draw(6, 1, 0, 0);
                cmd.EndRenderPass();
            }
        };

        fullScreenPassNode = graph.AddNode(GetCustomName())
                                 .InputTexture("input color", 1, Gfx::PipelineStage::Fragment_Shader)
                                 .InputRT("target color", 0)
                                 .AddColor(0)
                                 .SetExecFunc(execFunc)
                                 .Finish();

        return {Resource(ResourceTag::RenderGraphLink{}, 0, fullScreenPassNode, 0)};
    }

    bool Build(RenderGraph::Graph& graph, Resources& resources) override
    {
        auto [sourceNode, sourceHandle] =
            resources.GetResource(ResourceTag::RenderGraphLink{}, inputPropertyIDs["source"]);
        auto [targetColor, targetHandle] =
            resources.GetResource(ResourceTag::RenderGraphLink{}, inputPropertyIDs["target color"]);

        return graph.Connect(sourceNode, sourceHandle, fullScreenPassNode, 1) &&
               graph.Connect(targetColor, targetHandle, fullScreenPassNode, 0);
    };

    void Finalize(RenderGraph::Graph& graph, Resources& resources) override
    {
        auto [sourceNode, sourceHandle] =
            resources.GetResource(ResourceTag::RenderGraphLink{}, inputPropertyIDs["source"]);

        Gfx::Image* image = (Gfx::Image*)sourceNode->GetPass()->GetResourceRef(sourceHandle)->GetResource();
        passResource->SetImage("source", image);
    }

    void Execute(GraphResource& graphResource) override {}

private:
    RenderGraph::RenderNode* fullScreenPassNode;
    const DrawList* drawList;
    std::unique_ptr<Gfx::ShaderResource> passResource;

    void DefineNode()
    {
        AddConfig<ConfigurableType::ObjectPtr>("shader", nullptr);
        AddConfig<ConfigurableType::Vec2Int>("size", glm::ivec2{512.0f, 512.0f});

        AddInputProperty("source", PropertyType::RenderGraphLink);
        AddInputProperty("target color", PropertyType::RenderGraphLink);
        AddOutputProperty("color", PropertyType::RenderGraphLink);
    }
    static char _reg;
}; // namespace Engine::FrameGraph

char FullScreenPassNode::_reg = NodeBlueprintRegisteration::Register<FullScreenPassNode>("Full Screen Pass");

DEFINE_OBJECT(FullScreenPassNode, "F52A89D5-D830-4DD2-84DF-6A82A5F9F4CD");
} // namespace Engine::FrameGraph
