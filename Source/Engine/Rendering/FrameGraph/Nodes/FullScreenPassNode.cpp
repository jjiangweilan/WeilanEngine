#pragma once
#include "../NodeBlueprint.hpp"
#include "Asset/Shader.hpp"

namespace FrameGraph
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

    void Compile() override
    {
        shader = GetConfigurableVal<Shader*>("shader");
        passResource = GetGfxDriver()->CreateShaderResource();

        ClearConfigs();
        AddConfig<ConfigurableType::ObjectPtr>("shader", shader);

        if (shader)
        {
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
        }

        Gfx::ClearValue v;
        v.color.float32[0] = 0;
        v.color.float32[1] = 0;
        v.color.float32[2] = 0;
        v.color.float32[3] = 0;

        clears = {v};
    }

    std::vector<Resource> Preprocess(RenderGraph::Graph& graph) override
    {

        return {Resource(ResourceTag::RenderGraphLink{}, outputPropertyIDs["color"], fullScreenPassNode, 0)};
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

    void Execute(Gfx::CommandBuffer& cmd, RenderingData& renderingData) override
    {
        auto id = input.source->GetValue<AttachmentProperty>().id;
        auto targetColor = input.targetColor->GetValue<AttachmentProperty>().id;
        if (shader != nullptr)
        {
            mainPass.SetAttachment(0, targetColor);
            cmd.BeginRenderPass(mainPass, clears);
            cmd.BindShaderProgram(shader->GetDefaultShaderProgram(), shader->GetDefaultShaderConfig());
            cmd.BindResource(1, passResource.get());
            cmd.SetTexture(sourceHandle, id);
            cmd.Draw(6, 1, 0, 0);
            cmd.EndRenderPass();
        }

        output.color->SetValue(input.targetColor->GetValue<AttachmentProperty>());
    }

private:
    Shader* shader;
    std::vector<Gfx::ClearValue> clears;
    RenderGraph::RenderNode* fullScreenPassNode;
    const DrawList* drawList;
    std::unique_ptr<Gfx::ShaderResource> passResource;
    Gfx::RG::RenderPass mainPass = Gfx::RG::RenderPass::SingleColor("full screen pass");
    Gfx::ResourceHandle sourceHandle = Gfx::ResourceHandle("source");

    struct
    {
        PropertyHandle source;
        PropertyHandle targetColor;
    } input;

    struct
    {
        PropertyHandle color;
    } output;

    void DefineNode()
    {
        AddConfig<ConfigurableType::ObjectPtr>("shader", nullptr);
        AddConfig<ConfigurableType::Vec2Int>("size", glm::ivec2{512.0f, 512.0f});

        input.source = AddInputProperty("source", PropertyType::Attachment);
        input.targetColor = AddInputProperty("target color", PropertyType::Attachment);
        output.color = AddOutputProperty("color", PropertyType::Attachment);
    }
    static char _reg;
}; // namespace FrameGraph

char FullScreenPassNode::_reg = NodeBlueprintRegisteration::Register<FullScreenPassNode>("Full Screen Pass");

DEFINE_OBJECT(FullScreenPassNode, "F52A89D5-D830-4DD2-84DF-6A82A5F9F4CD");
} // namespace FrameGraph
