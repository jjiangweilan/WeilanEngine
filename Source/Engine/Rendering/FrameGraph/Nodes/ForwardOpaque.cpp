#pragma once
#include "../NodeBlueprint.hpp"
#include "Asset/Shader.hpp"

namespace Engine::FrameGraph
{
class ForwardOpaqueNode : public Node
{
    DECLARE_OBJECT();

public:
    ForwardOpaqueNode()
    {
        DefineNode();
    };
    ForwardOpaqueNode(FGID id) : Node("Forward Opaque", id)
    {
        DefineNode();
    }

    std::vector<Resource> Preprocess(RenderGraph::Graph& graph) override
    {
        Shader* opaqueShader = GetConfigurableVal<Shader*>("scene info shader");
        sceneShaderResource = Gfx::GfxDriver::Instance()->CreateShaderResource(
            opaqueShader->GetShaderProgram(),
            Gfx::ShaderResourceFrequency::Global
        );
        glm::vec4 clearValuesVal = GetConfigurableVal<glm::vec4>("clear values");
        std::vector<Gfx::ClearValue> clearValues = {
            {.color = {{clearValuesVal[0], clearValuesVal[1], clearValuesVal[2], clearValuesVal[3]}}},
            {.depthStencil = {.depth = 1}}};

        forwardNode = graph.AddNode2(
            {
                {
                    .name = "shadow",
                    .handle = RenderGraph::StrToHandle("shadow"),
                    .type = RenderGraph::PassDependencyType::Texture,
                    .stageFlags = Gfx::PipelineStage::Fragment_Shader,
                },
            },
            {{
                .colors = {{
                    .name = "opaque color",
                    .handle = RenderGraph::StrToHandle("opaque color"),
                    .create = false,
                    .loadOp = Gfx::AttachmentLoadOperation::Clear,
                    .storeOp = Gfx::AttachmentStoreOperation::Store,
                }},
                .depth = {{
                    .name = "opaque depth",
                    .handle = RenderGraph::StrToHandle("opaque depth"),
                    .create = false,
                    .loadOp = Gfx::AttachmentLoadOperation::Clear,
                    .storeOp = Gfx::AttachmentStoreOperation::Store,
                }},
            }},
            [this, clearValues](Gfx::CommandBuffer& cmd, Gfx::RenderPass& pass, const RenderGraph::ResourceRefs& res)
            {
                Gfx::Image* color = (Gfx::Image*)res.at(0)->GetResource();
                uint32_t width = color->GetDescription().width;
                uint32_t height = color->GetDescription().height;
                cmd.SetViewport(
                    {.x = 0, .y = 0, .width = (float)width, .height = (float)height, .minDepth = 0, .maxDepth = 1}
                );
                Rect2D rect = {{0, 0}, {width, height}};
                cmd.SetScissor(0, 1, &rect);
                cmd.BindResource(sceneShaderResource);
                cmd.BeginRenderPass(pass, clearValues);

                // draw scene objects
                for (auto& draw : *drawList)
                {
                    cmd.BindShaderProgram(draw.shader, *draw.shaderConfig);
                    cmd.BindVertexBuffer(draw.vertexBufferBinding, 0);
                    cmd.BindIndexBuffer(draw.indexBuffer, 0, draw.indexBufferType);
                    cmd.BindResource(draw.shaderResource);
                    cmd.SetPushConstant(draw.shader, (void*)&draw.pushConstant);
                    cmd.DrawIndexed(draw.indexCount, 1, 0, 0, 0);
                }

                // draw skybox
                // auto& cubeMesh = cube->GetMeshes()[0];
                // CmdDrawSubmesh(cmd, *cubeMesh, 0, *skyboxShader, *skyboxPassResource);

                cmd.EndRenderPass();
            }
        );
        forwardNode->SetName(GetCustomName());

        return {
            Resource(
                ResourceTag::RenderGraphLink{},
                propertyIDs["color"],
                forwardNode,
                RenderGraph::StrToHandle("opaque color")
            ),
            Resource(
                ResourceTag::RenderGraphLink{},
                propertyIDs["depth"],
                forwardNode,
                RenderGraph::StrToHandle("opaque depth")
            )};
    }

    void Build(RenderGraph::Graph& graph, Resources& resources) override
    {
        auto [colorNode, colorHandle] = resources.GetResource(ResourceTag::RenderGraphLink{}, propertyIDs["color"]);
        auto [depthNode, depthHandle] = resources.GetResource(ResourceTag::RenderGraphLink{}, propertyIDs["depth"]);
        auto [shadowNode, shadowHandle] =
            resources.GetResource(ResourceTag::RenderGraphLink{}, propertyIDs["shadow map"]);
        drawList = resources.GetResource(ResourceTag::DrawList{}, propertyIDs["draw list"]);

        graph.Connect(colorNode, colorHandle, forwardNode, RenderGraph::StrToHandle("opaque color"));
        graph.Connect(depthNode, depthHandle, forwardNode, RenderGraph::StrToHandle("opaque depth"));
        graph.Connect(shadowNode, shadowHandle, forwardNode, RenderGraph::StrToHandle("shadow map"));
    };

    void Finalize(RenderGraph::Graph& graph, Resources& resources) override
    {
        auto [shadowNode, shadowHandle] =
            resources.GetResource(ResourceTag::RenderGraphLink{}, propertyIDs["shadow map"]);
        Gfx::Image* shadowImage = (Gfx::Image*)shadowNode->GetPass()->GetResourceRef(shadowHandle)->GetResource();
        sceneShaderResource->SetImage("shadowMap", shadowImage);
    }

private:
    RenderGraph::RenderNode* forwardNode;
    std::unique_ptr<Gfx::ShaderResource> sceneShaderResource{};
    const DrawList* drawList;

    void DefineNode()
    {
        AddInputProperty("color", PropertyType::Image);
        AddInputProperty("depth", PropertyType::Image);
        AddInputProperty("shadow map", PropertyType::Image);
        AddInputProperty("draw list", PropertyType::DrawList);

        AddConfig<ConfigurableType::Vec4>("clear values", glm::vec4{52 / 255.0f, 177 / 255.0f, 235 / 255.0f, 1});
        AddConfig<ConfigurableType::ObjectPtr>("scene info shader", nullptr);
    }
    static char _reg;
}; // namespace Engine::FrameGraph

char ForwardOpaqueNode::_reg = NodeBlueprintRegisteration::Register<ForwardOpaqueNode>("Forward Opaque");

DEFINE_OBJECT(ForwardOpaqueNode, "E6188926-D83E-4B17-9C7C-060A5862BDCA");
} // namespace Engine::FrameGraph
