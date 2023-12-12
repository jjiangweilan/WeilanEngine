#pragma once
#include "../NodeBlueprint.hpp"
#include "Asset/Shader.hpp"
#include "GfxDriver/Image.hpp"

namespace Engine::FrameGraph
{
class ShadowMapNode : public Node
{
    DECLARE_OBJECT();

public:
    ShadowMapNode()
    {
        DefineNode();
    };
    ShadowMapNode(FGID id) : Node("Shadow Map", id)
    {
        DefineNode();
    }

    std::vector<Resource> Preprocess(RenderGraph::Graph& graph) override
    {
        glm::vec2 shadowMapSize = GetConfigurableVal<glm::vec2>("shadow map size");
        Shader* shadowmapShader = GetConfigurableVal<Shader*>("shadow map shader");
        Gfx::ShaderProgram* shadowmapShaderProgram = shadowmapShader->GetShaderProgram({});
        std::vector<Gfx::ClearValue> shadowMapClears = {{.depthStencil = {1}}};

        shadowMapPass = graph.AddNode2(
            {},
            {
                {
                    .width = (uint32_t)shadowMapSize.x,
                    .height = (uint32_t)shadowMapSize.y,
                    .depth =
                        RenderGraph::Attachment{
                            .name = "shadow depth",
                            .handle = 2,
                            .create = true,
                            .format = Gfx::ImageFormat::D32_SFloat,
                            .storeOp = Gfx::AttachmentStoreOperation::Store,
                        },
                },
            },
            [this,
             shadowMapClears,
             shadowmapShaderProgram,
             shadowMapSize](Gfx::CommandBuffer& cmd, Gfx::RenderPass& pass, const RenderGraph::ResourceRefs& ref)
            {
                cmd.SetViewport(
                    {.x = 0, .y = 0, .width = shadowMapSize.x, .height = shadowMapSize.y, .minDepth = 0, .maxDepth = 1}
                );
                Rect2D rect = {{0, 0}, {(uint32_t)shadowMapSize.x, (uint32_t)shadowMapSize.y}};
                cmd.SetScissor(0, 1, &rect);
                cmd.BeginRenderPass(pass, shadowMapClears);

                for (auto& draw : *drawList)
                {
                    cmd.BindShaderProgram(shadowmapShaderProgram, shadowmapShaderProgram->GetDefaultShaderConfig());
                    cmd.BindVertexBuffer(draw.vertexBufferBinding, 0);
                    cmd.BindIndexBuffer(draw.indexBuffer, 0, draw.indexBufferType);
                    cmd.SetPushConstant(shadowmapShaderProgram, (void*)&draw.pushConstant);
                    cmd.DrawIndexed(draw.indexCount, 1, 0, 0, 0);
                }

                cmd.EndRenderPass();
            }
        );

        return {
            Resource(ResourceTag::RenderGraphLink{}, outputPropertyIDs["shadow map"], shadowMapPass, 2),
        };
    };

    void ProcessSceneShaderResource(Gfx::ShaderResource& sceneShaderResource) override
    {
        Gfx::Image* shadowImage = (Gfx::Image*)shadowMapPass->GetPass()->GetResourceRef(2)->GetResource();
        sceneShaderResource.SetImage("shadowMap", shadowImage);
    }

    bool Build(RenderGraph::Graph& graph, Resources& resources) override
    {
        drawList = resources.GetResource(ResourceTag::DrawList{}, inputPropertyIDs["draw list"]);

        if (drawList == nullptr)
            return false;

        return true;
    };

private:
    RenderGraph::RenderNode* shadowMapPass;
    RenderGraph::RenderNode* vsmBoxFilterPass0;

    const DrawList* drawList;
    void DefineNode()
    {
        AddOutputProperty("shadow map", PropertyType::RenderGraphLink);
        AddInputProperty("draw list", PropertyType::DrawList);

        AddConfig<ConfigurableType::Vec2>("shadow map size", glm::vec2{1024, 1024});
        AddConfig<ConfigurableType::ObjectPtr>("shadow map shader", nullptr);
    }
    static char _reg;
};

char ShadowMapNode::_reg = NodeBlueprintRegisteration::Register<ShadowMapNode>("Shadow Map");
DEFINE_OBJECT(ShadowMapNode, "A486879A-5EA3-456A-ACF0-ED3E536826A0");
} // namespace Engine::FrameGraph
