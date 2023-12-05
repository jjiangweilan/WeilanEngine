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
        glm::vec4* clearValuesVal = GetConfigurablePtr<glm::vec4>("clear values");

        auto opaqueColorHandle = RenderGraph::StrToHandle("opaque color");

        RenderGraph::ResourceHandle color = graph.AllocateRT("opaque color");
        RenderGraph::ResourceHandle depth = graph.AllocateRT("opaque depth");

        graph.SetViewport();
        graph.SetScissor();
        graph.BeginRenderPass({color}, depth);
        graph.BindResource(sceneShaderResource);
        graph.BindVertexBuffer();
        graph.EndRenderPass();

        forwardNode = graph.AddNode2(
            {
                {
                    .name = "shadow",
                    .handle = RenderGraph::StrToHandle("shadow map"),
                    .type = RenderGraph::PassDependencyType::Texture,
                    .stageFlags = Gfx::PipelineStage::Fragment_Shader,
                },
            },
            {{
                .colors = {{
                    .name = "opaque color",
                    .handle = opaqueColorHandle,
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
            [this,
             clearValuesVal,
             opaqueColorHandle](Gfx::CommandBuffer& cmd, Gfx::RenderPass& pass, const RenderGraph::ResourceRefs& res)
            {
                Gfx::Image* color = (Gfx::Image*)res.at(opaqueColorHandle)->GetResource();
                uint32_t width = color->GetDescription().width;
                uint32_t height = color->GetDescription().height;
                cmd.BindResource(sceneShaderResource);
                cmd.SetViewport(
                    {.x = 0, .y = 0, .width = (float)width, .height = (float)height, .minDepth = 0, .maxDepth = 1}
                );
                Rect2D rect = {{0, 0}, {width, height}};
                clearValues[0].color = {
                    {(*clearValuesVal)[0], (*clearValuesVal)[1], (*clearValuesVal)[2], (*clearValuesVal)[3]}};
                clearValues[1].depthStencil = {1};

                cmd.SetScissor(0, 1, &rect);
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
                outputPropertyIDs["color"],
                forwardNode,
                RenderGraph::StrToHandle("opaque color")
            ),
            Resource(
                ResourceTag::RenderGraphLink{},
                outputPropertyIDs["depth"],
                forwardNode,
                RenderGraph::StrToHandle("opaque depth")
            )};
    }

    void Build(RenderGraph::Graph& graph, Resources& resources) override
    {
        auto [colorNode, colorHandle] =
            resources.GetResource(ResourceTag::RenderGraphLink{}, inputPropertyIDs["color"]);
        auto [depthNode, depthHandle] =
            resources.GetResource(ResourceTag::RenderGraphLink{}, inputPropertyIDs["depth"]);
        auto [shadowNode, shadowHandle] =
            resources.GetResource(ResourceTag::RenderGraphLink{}, inputPropertyIDs["shadow map"]);
        drawList = resources.GetResource(ResourceTag::DrawList{}, inputPropertyIDs["draw list"]);

        graph.Connect(colorNode, colorHandle, forwardNode, RenderGraph::StrToHandle("opaque color"));
        graph.Connect(depthNode, depthHandle, forwardNode, RenderGraph::StrToHandle("opaque depth"));
        graph.Connect(shadowNode, shadowHandle, forwardNode, RenderGraph::StrToHandle("shadow map"));
    };

    void ProcessSceneShaderResource(Gfx::ShaderResource& sceneShaderResource) override
    {
        this->sceneShaderResource = &sceneShaderResource;
    };

    void Execute(GraphResource& graphResource) override {}

private:
    RenderGraph::RenderNode* forwardNode;
    const DrawList* drawList;
    Gfx::ShaderResource* sceneShaderResource;
    std::vector<Gfx::ClearValue> clearValues;

    void DefineNode()
    {
        AddInputProperty("color", PropertyType::RenderGraphLink);
        AddInputProperty("depth", PropertyType::RenderGraphLink);
        AddInputProperty("shadow map", PropertyType::RenderGraphLink);
        AddInputProperty("draw list", PropertyType::DrawList);

        AddOutputProperty("color", PropertyType::RenderGraphLink);
        AddOutputProperty("depth", PropertyType::RenderGraphLink);

        AddConfig<ConfigurableType::Vec4>("clear values", glm::vec4{52 / 255.0f, 177 / 255.0f, 235 / 255.0f, 1});
        clearValues.resize(2);
    }
    static char _reg;
}; // namespace Engine::FrameGraph

char ForwardOpaqueNode::_reg = NodeBlueprintRegisteration::Register<ForwardOpaqueNode>("Forward Opaque");

DEFINE_OBJECT(ForwardOpaqueNode, "E6188926-D83E-4B17-9C7C-060A5862BDCA");
} // namespace Engine::FrameGraph
