#pragma once
#include "../NodeBlueprint.hpp"
#include "GfxDriver/GfxEnums.hpp"
#include <glm/glm.hpp>

namespace Engine::FrameGraph
{
class ImageNode : public Node
{
    DECLARE_OBJECT();

public:
    ImageNode()
    {
        DefineNode();
    }

    ImageNode(FGID id) : Node("Image", id)
    {
        DefineNode();
    }

    std::vector<Resource> Preprocess(RenderGraph::Graph& graph) override
    {
        RenderGraph::RenderNode* imageNode = graph.AddNode(
            [](Gfx::CommandBuffer&, Gfx::RenderPass&, const RenderGraph::ResourceRefs&) {},
            {{
                .name = GetCustomName(),
                .handle = 0,
                .type = RenderGraph::ResourceType::Image,
                .accessFlags = Gfx::AccessMask::None,
                .stageFlags = Gfx::PipelineStage::None,
                .imageUsagesFlags = Gfx::ImageUsage::ColorAttachment | Gfx::ImageUsage::Texture,
                .imageLayout = Gfx::ImageLayout::Color_Attachment,
            }},
            {}
        );

        return {
            Resource(ResourceTag::RenderGraphLink{}, propertyIDs["image"], imageNode, 0),
        };
    };
    void Build(RenderGraph::Graph& graph, Resources& resources) override{};

private:
    void DefineNode()
    {
        AddOutputProperty("image", PropertyType::Image);

        AddConfig<ConfigurableType::Vec2Int>("size", glm::ivec2{512.0f, 512.0f});
        AddConfig<ConfigurableType::Format>("format", Gfx::ImageFormat::R8G8B8A8_UNorm);
        AddConfig<ConfigurableType::Int>("mip level", int{1});
    }
    static char _reg;
};

char ImageNode::_reg = NodeBlueprintRegisteration::Register<ImageNode>("Image");
DEFINE_OBJECT(ImageNode, "FE0667ED-89FA-4986-842B-158654543C18");
} // namespace Engine::FrameGraph
