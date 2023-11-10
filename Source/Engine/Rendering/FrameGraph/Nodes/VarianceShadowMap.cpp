#pragma once
#include "../NodeBlueprint.hpp"
#include "Asset/Shader.hpp"
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

    std::vector<Resource> Preprocess(RenderGraph::Graph& graph) override
    {
        glm::vec2 shadowMapSize = GetConfigurableVal<glm::vec2>("shadow map size");
        Shader* shadowmapShader = GetConfigurableVal<Shader*>("shadow map shader");
        boxFilterShader = GetConfigurableVal<Shader*>("two pass filter shader");
        std::vector<Gfx::ClearValue> vsmClears = {{.color = {{1, 1, 1, 1}}}, {.depthStencil = {1}}};

        vsmPass = graph.AddNode2(
            {},
            {
                {
                    .width = (uint32_t)shadowMapSize.x,
                    .height = (uint32_t)shadowMapSize.y,
                    .colors =
                        {
                            RenderGraph::Attachment{
                                .name = "shadow map",
                                .handle = 0,
                                .create = true,
                                .format = Gfx::ImageFormat::R32G32_SFloat,
                                .imageView = RenderGraph::ImageView{Gfx::ImageAspect::Color, 0},
                                .multiSampling = Gfx::MultiSampling::Sample_Count_1,
                                .mipLevels = 1,
                                .loadOp = Gfx::AttachmentLoadOperation::Clear,
                                .storeOp = Gfx::AttachmentStoreOperation::Store,
                            },
                        },
                    .depth =
                        RenderGraph::Attachment{
                            .name = "shadow depth",
                            .handle = 2,
                            .create = true,
                            .format = Gfx::ImageFormat::D24_UNorm_S8_UInt,
                            .storeOp = Gfx::AttachmentStoreOperation::DontCare,
                        },
                },
            },
            [this,
             vsmClears,
             shadowmapShader,
             shadowMapSize](Gfx::CommandBuffer& cmd, Gfx::RenderPass& pass, const RenderGraph::ResourceRefs& ref)
            {
                cmd.SetViewport(
                    {.x = 0, .y = 0, .width = shadowMapSize.x, .height = shadowMapSize.y, .minDepth = 0, .maxDepth = 1}
                );
                Rect2D rect = {{0, 0}, {(uint32_t)shadowMapSize.x, (uint32_t)shadowMapSize.y}};
                cmd.SetScissor(0, 1, &rect);
                cmd.BeginRenderPass(pass, vsmClears);

                for (auto& draw : *drawList)
                {
                    cmd.BindShaderProgram(
                        shadowmapShader->GetDefaultShaderProgram(),
                        shadowmapShader->GetDefaultShaderConfig()
                    );
                    cmd.BindVertexBuffer(draw.vertexBufferBinding, 0);
                    cmd.BindIndexBuffer(draw.indexBuffer, 0, draw.indexBufferType);
                    cmd.SetPushConstant(shadowmapShader->GetDefaultShaderProgram(), (void*)&draw.pushConstant);
                    cmd.DrawIndexed(draw.indexCount, 1, 0, 0, 0);
                }

                cmd.EndRenderPass();
            }
        );

        std::vector<Gfx::ClearValue> boxFilterClears = {{.color = {{1, 1, 1, 1}}}};
        vsmBoxFilterPass0 = graph.AddNode2(
            {
                RenderGraph::PassDependency{
                    .name = "box filter source",
                    .handle = RenderGraph::StrToHandle("src"),
                    .type = RenderGraph::PassDependencyType::Texture,
                    .stageFlags = Gfx::PipelineStage::Fragment_Shader,
                },
            },
            {
                RenderGraph::Subpass{
                    .width = (uint32_t)shadowMapSize.x,
                    .height = (uint32_t)shadowMapSize.y,
                    .colors =
                        {
                            RenderGraph::Attachment{
                                .handle = RenderGraph::StrToHandle("dst"),
                                .create = true,
                                .format = Gfx::ImageFormat::R32G32_SFloat,
                            },

                        },
                },
            },
            [=](Gfx::CommandBuffer& cmd, auto& pass, auto& refs)
            {
                uint32_t width = shadowMapSize.x;
                uint32_t height = shadowMapSize.y;
                cmd.SetViewport(
                    {.x = 0, .y = 0, .width = (float)width, .height = (float)height, .minDepth = 0, .maxDepth = 1}
                );
                Rect2D rect = {{0, 0}, {width, height}};
                cmd.SetScissor(0, 1, &rect);
                cmd.BeginRenderPass(pass, boxFilterClears);
                cmd.BindResource(vsmBoxFilterResource0);
                cmd.BindShaderProgram(boxFilterShader->GetDefaultShaderProgram(), boxFilterShader->GetDefaultShaderConfig());
                struct
                {
                    glm::vec4 textureSize;
                    glm::vec4 xory;
                } pval;
                pval.textureSize =
                    glm::vec4(shadowMapSize.x, shadowMapSize.y, 1.0f / shadowMapSize.x, 1.0f / shadowMapSize.y);
                pval.xory = glm::vec4(0);
                cmd.SetPushConstant(boxFilterShader->GetDefaultShaderProgram(), &pval);
                cmd.Draw(6, 1, 0, 0);
                cmd.EndRenderPass();
            }
        );
        vsmPass->SetName("vsm Pass");
        vsmBoxFilterPass0->SetName("vsm Box Filter Pass 0");
        graph.Connect(vsmPass, 0, vsmBoxFilterPass0, RenderGraph::StrToHandle("src"));

        auto vsmBoxFilterPass1 = graph.AddNode2(
            {
                RenderGraph::PassDependency{
                    .name = "box filter source",
                    .handle = RenderGraph::StrToHandle("src"),
                    .type = RenderGraph::PassDependencyType::Texture,
                    .stageFlags = Gfx::PipelineStage::Fragment_Shader,
                },
            },
            {
                RenderGraph::Subpass{
                    .width = (uint32_t)shadowMapSize.x,
                    .height = (uint32_t)shadowMapSize.y,
                    .colors =
                        {
                            RenderGraph::Attachment{
                                .handle = RenderGraph::StrToHandle("dst"),
                                .create = false,
                                .format = Gfx::ImageFormat::R32G32_SFloat,
                                .imageView = RenderGraph::ImageView{Gfx::ImageAspect::Color, 0},
                            },

                        },
                },
            },
            [=](Gfx::CommandBuffer& cmd, auto& pass, auto& refs)
            {
                uint32_t width = shadowMapSize.x;
                uint32_t height = shadowMapSize.y;
                cmd.SetViewport(
                    {.x = 0, .y = 0, .width = (float)width, .height = (float)height, .minDepth = 0, .maxDepth = 1}
                );
                Rect2D rect = {{0, 0}, {width, height}};
                cmd.SetScissor(0, 1, &rect);
                cmd.BeginRenderPass(pass, boxFilterClears);
                struct
                {
                    glm::vec4 textureSize;
                    glm::vec4 xory;
                } pval;
                pval.textureSize =
                    glm::vec4(shadowMapSize.x, shadowMapSize.y, 1.0f / shadowMapSize.x, 1.0f / shadowMapSize.y);
                pval.xory = glm::vec4(1);
                cmd.BindResource(vsmBoxFilterResource1);
                cmd.BindShaderProgram(boxFilterShader->GetDefaultShaderProgram(), boxFilterShader->GetDefaultShaderConfig());
                cmd.SetPushConstant(boxFilterShader->GetDefaultShaderProgram(), &pval);
                cmd.Draw(6, 1, 0, 0);
                cmd.EndRenderPass();
            }
        );
        graph.Connect(vsmPass, 0, vsmBoxFilterPass1, RenderGraph::StrToHandle("dst"));
        graph.Connect(
            vsmBoxFilterPass0,
            RenderGraph::StrToHandle("dst"),
            vsmBoxFilterPass1,
            RenderGraph::StrToHandle("src")
        );
        vsmBoxFilterPass1->SetName("vsm Box Filter Pass 1");

        return {
            Resource(
                ResourceTag::RenderGraphLink{},
                propertyIDs["shadow map"],
                vsmBoxFilterPass1,
                RenderGraph::StrToHandle("dst")
            ),
        };
    };

    virtual void Finalize(RenderGraph::Graph& graph, Resources& resources) override
    {
        Gfx::Image* shadowImage = (Gfx::Image*)vsmPass->GetPass()->GetResourceRef(0)->GetResource();
        Gfx::Image* vsmBoxFilterPass0Image =
            (Gfx::Image*)vsmBoxFilterPass0->GetPass()->GetResourceRef(RenderGraph::StrToHandle("dst"))->GetResource();

        vsmBoxFilterResource0 = GetGfxDriver()->CreateShaderResource(
            boxFilterShader->GetDefaultShaderProgram(),
            Gfx::ShaderResourceFrequency::Material
        );
        vsmBoxFilterResource1 = GetGfxDriver()->CreateShaderResource(
            boxFilterShader->GetDefaultShaderProgram(),
            Gfx::ShaderResourceFrequency::Material
        );
        vsmBoxFilterResource0->SetImage("source", shadowImage);
        vsmBoxFilterResource1->SetImage("source", vsmBoxFilterPass0Image);
    }

    void ProcessSceneShaderResource(Gfx::ShaderResource& sceneShaderResource) override
    {
        Gfx::Image* shadowImage = (Gfx::Image*)vsmPass->GetPass()->GetResourceRef(0)->GetResource();
        sceneShaderResource.SetImage("shadowMap", shadowImage);
    }

    void Build(RenderGraph::Graph& graph, Resources& resources) override
    {
        drawList = resources.GetResource(ResourceTag::DrawList{}, propertyIDs["draw list"]);
    };

private:
    RenderGraph::RenderNode* vsmPass;
    RenderGraph::RenderNode* vsmBoxFilterPass0;

    const DrawList* drawList;
    std::unique_ptr<Gfx::ShaderResource> vsmBoxFilterResource0{};
    std::unique_ptr<Gfx::ShaderResource> vsmBoxFilterResource1{};
    Shader* boxFilterShader;

    void DefineNode()
    {
        AddOutputProperty("shadow map", PropertyType::Image);
        AddInputProperty("draw list", PropertyType::DrawList);

        AddConfig<ConfigurableType::Vec2>("shadow map size", glm::vec2{1024, 1024});
        AddConfig<ConfigurableType::ObjectPtr>("shadow map shader", nullptr);
        AddConfig<ConfigurableType::ObjectPtr>("two pass filter shader", nullptr);
    }
    static char _reg;
};

char VarianceShadowMapNode::_reg = NodeBlueprintRegisteration::Register<VarianceShadowMapNode>("Variance Shadow Map");
DEFINE_OBJECT(VarianceShadowMapNode, "BED9EF19-14FD-4FE9-959B-2410E44A5669");
} // namespace Engine::FrameGraph
