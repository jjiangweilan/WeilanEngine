#pragma once
#include "Asset/Shader.hpp"
#include "Rendering/FrameGraph/NodeBlueprint.hpp"

namespace FrameGraph
{
class SurfelBakeFGNode : public Node
{
    DECLARE_OBJECT();

public:
    SurfelBakeFGNode()
    {
        DefineNode();
    };
    SurfelBakeFGNode(FGID id) : Node("Surfel Bake Node", id)
    {
        DefineNode();
    }

    FGID inputDrawList;
    Gfx::Image* albedo;
    Gfx::Image* normal;
    Gfx::Image* position;
    const uint32_t size = 512;
    const uint32_t mipLevel = glm::log2((float)size) + 1;

    std::vector<Resource> Preprocess(RenderGraph::Graph& graph) override
    {
        std::vector<Gfx::ClearValue> clearValues = {
            Gfx::ClearValue{0, 0, 0, 0},
            Gfx::ClearValue{0, 0, 0, 0},
            Gfx::ClearValue{0, 0, 0, 0},
            Gfx::ClearValue{1, 0},
        };

        RenderGraph::RenderPass::ImageView v{
            .imageViewType = Gfx::ImageViewType::Image_2D,
            .subresourceRange = {
                .aspectMask = Gfx::ImageAspectFlags::Color,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            }};
        surfelDrawScene = graph.AddNode(
            [=](Gfx::CommandBuffer& cmd, Gfx::RenderPass& pass, const RenderGraph::ResourceRefs& res)
            {
                Gfx::Image* color = (Gfx::Image*)res.at(0)->GetResource();
                uint32_t width = color->GetDescription().width;
                uint32_t height = color->GetDescription().height;
                cmd.BindResource(0, sceneShaderResource);
                cmd.SetViewport(
                    {.x = 0, .y = 0, .width = (float)width, .height = (float)height, .minDepth = 0, .maxDepth = 1}
                );
                Rect2D rect = {{0, 0}, {width, height}};

                cmd.SetScissor(0, 1, &rect);

                cmd.BeginRenderPass(pass, clearValues);
                for (auto& draw : *drawList)
                {
                    cmd.BindShaderProgram(draw.shader, *draw.shaderConfig);
                    cmd.BindVertexBuffer(draw.vertexBufferBinding, 0);
                    cmd.BindIndexBuffer(draw.indexBuffer, 0, draw.indexBufferType);
                    cmd.BindResource(2, draw.shaderResource);
                    cmd.SetPushConstant(draw.shader, (void*)&draw.pushConstant);
                    cmd.DrawIndexed(draw.indexCount, 1, 0, 0, 0);
                }
                cmd.EndRenderPass();

                GenerateMip(cmd, albedo, Gfx::ImageAspect::Color);
                GenerateMip(cmd, normal, Gfx::ImageAspect::Color);
                GenerateMip(cmd, position, Gfx::ImageAspect::Color);
            },
            {
                {
                    .name = "surfel color",
                    .handle = 0,
                    .type = RenderGraph::ResourceType::Image,
                    .accessFlags = Gfx::AccessMask::None,
                    .stageFlags = Gfx::PipelineStage::Top_Of_Pipe,
                    .imageUsagesFlags = Gfx::ImageUsage::Texture | Gfx::ImageUsage::TransferSrc |
                                        Gfx::ImageUsage::TransferDst | Gfx::ImageUsage::ColorAttachment,
                    .imageLayout = Gfx::ImageLayout::Color_Attachment,
                    .imageCreateInfo = Gfx::ImageDescription(
                        size,
                        size,
                        1,
                        Gfx::ImageFormat::R32G32B32A32_SFloat,
                        Gfx::MultiSampling::Sample_Count_1,
                        mipLevel,
                        false
                    ),
                },
                {
                    .name = "surfel normal",
                    .handle = 1,
                    .type = RenderGraph::ResourceType::Image,
                    .accessFlags = Gfx::AccessMask::Color_Attachment_Write,
                    .stageFlags = Gfx::PipelineStage::Color_Attachment_Output,
                    .imageUsagesFlags = Gfx::ImageUsage::Texture | Gfx::ImageUsage::TransferSrc |
                                        Gfx::ImageUsage::TransferDst | Gfx::ImageUsage::ColorAttachment,
                    .imageLayout = Gfx::ImageLayout::Color_Attachment,
                    .imageCreateInfo =
                        {
                            size,
                            size,
                            1,
                            Gfx::ImageFormat::R32G32B32A32_SFloat,
                            Gfx::MultiSampling::Sample_Count_1,
                            mipLevel,
                            false,
                        },
                },
                {
                    .name = "surfel position",
                    .handle = 2,
                    .type = RenderGraph::ResourceType::Image,
                    .accessFlags = Gfx::AccessMask::Color_Attachment_Write,
                    .stageFlags = Gfx::PipelineStage::Color_Attachment_Output,
                    .imageUsagesFlags = Gfx::ImageUsage::Texture | Gfx::ImageUsage::TransferSrc |
                                        Gfx::ImageUsage::TransferDst | Gfx::ImageUsage::ColorAttachment,
                    .imageLayout = Gfx::ImageLayout::Color_Attachment,
                    .imageCreateInfo =
                        {
                            size,
                            size,
                            1,
                            Gfx::ImageFormat::R32G32B32A32_SFloat,
                            Gfx::MultiSampling::Sample_Count_1,
                            mipLevel,
                            false,
                        },
                },
                {
                    .name = "surfel depth",
                    .handle = 3,
                    .type = RenderGraph::ResourceType::Image,
                    .accessFlags = Gfx::AccessMask::Depth_Stencil_Attachment_Read |
                                   Gfx::AccessMask::Depth_Stencil_Attachment_Write,
                    .stageFlags = Gfx::PipelineStage::Early_Fragment_Tests,
                    .imageUsagesFlags = Gfx::ImageUsage::Texture | Gfx::ImageUsage::TransferSrc |
                                        Gfx::ImageUsage::TransferDst | Gfx::ImageUsage::DepthStencilAttachment,
                    .imageLayout = Gfx::ImageLayout::Depth_Stencil_Attachment,
                    .imageCreateInfo =
                        {
                            size,
                            size,
                            1,
                            Gfx::ImageFormat::D32_SFloat,
                            Gfx::MultiSampling::Sample_Count_1,
                            1,
                            false,
                        },
                },
            },
            {
                RenderGraph::RenderPass::Subpass{
                    .colors =
                        {
                            RenderGraph::RenderPass::Attachment{.handle = 0, .imageView = v},
                            RenderGraph::RenderPass::Attachment{.handle = 1, .imageView = v},
                            RenderGraph::RenderPass::Attachment{.handle = 2, .imageView = v},
                        },
                    .depth = {{.handle = 3}},
                },
            }
        );

        return {};
    }

    bool Build(RenderGraph::Graph& graph, Resources& resources) override
    {
        drawList = resources.GetResource(ResourceTag::DrawList{}, inputPropertyIDs["draw list"]);

        if (drawList == nullptr)
            return false;
        return true;
    };

    void ProcessSceneShaderResource() override{};

    void Finalize(RenderGraph::Graph& graph, Resources& resources) override
    {
        albedo = (Gfx::Image*)surfelDrawScene->GetPass()->GetResourceRef(0)->GetResource();
        normal = (Gfx::Image*)surfelDrawScene->GetPass()->GetResourceRef(1)->GetResource();
        position = (Gfx::Image*)surfelDrawScene->GetPass()->GetResourceRef(2)->GetResource();

        Shader::EnableFeature("G_SURFEL_BAKE");
    }

    void OnDestroy() override
    {
        Shader::DisableFeature("G_SURFEL_BAKE");
    }

private:
    RenderGraph::RenderNode* surfelDrawScene;
    const DrawList* drawList;
    Gfx::ShaderResource* sceneShaderResource;

    void DefineNode()
    {
        inputDrawList = AddInputProperty("draw list", PropertyType::DrawList);
    }

    void GenerateMip(Gfx::CommandBuffer& cmd, Gfx::Image* image, Gfx::ImageAspect aspect)
    {
        for (uint32_t mip = 1; mip < mipLevel; ++mip)
        {
            Gfx::BlitOp blitOp = {
                .srcMip = {mip - 1},
                .dstMip = {mip},
            };
            cmd.Blit(image, image, blitOp);
        }
    }
    static char _reg;
}; // namespace FrameGraph
} // namespace FrameGraph
