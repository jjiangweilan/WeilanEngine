#pragma once
#include "ImageNode.hpp"

namespace Engine::FrameGraph
{
DEFINE_OBJECT(ImageNode, "FE0667ED-89FA-4986-842B-158654543C18");

ImageNode::ImageNode()
{
    DefineNode();
}

ImageNode::ImageNode(FGID id) : Node("Image", id)
{
    DefineNode();
}

std::vector<Resource> ImageNode::Preprocess(RenderGraph::Graph& graph)
{
    glm::ivec2 size = GetConfigurableVal<glm::ivec2>("size");
    Gfx::ImageFormat format = GetConfigurableVal<Gfx::ImageFormat>("format");
    int mipLevel = GetConfigurableVal<int>("mip level");

    imageNode = graph.AddNode(
        [](Gfx::CommandBuffer&, Gfx::RenderPass&, const RenderGraph::ResourceRefs&) {},
        {{.name = GetCustomName(),
          .handle = 0,
          .type = RenderGraph::ResourceType::Image,
          .accessFlags = Gfx::AccessMask::None,
          .stageFlags = Gfx::PipelineStage::Top_Of_Pipe,
          .imageUsagesFlags = Gfx::ImageUsage::Texture,
          .imageLayout = Gfx::IsDepthStencilFormat(format) ? Gfx::ImageLayout::Depth_Stencil_Attachment
                                                           : Gfx::ImageLayout::Color_Attachment,
          .imageCreateInfo =
              {
                  .width = (uint32_t)size.x,
                  .height = (uint32_t)size.y,
                  .format = format,
                  .multiSampling = Gfx::MultiSampling::Sample_Count_1,
                  .mipLevels = (uint32_t)mipLevel,
                  .isCubemap = false,
              }}},
        {}
    );
    imageNode->SetName(GetCustomName());

    return {
        Resource(ResourceTag::RenderGraphLink{}, propertyIDs["image"], imageNode, 0),
    };
};

void ImageNode::DefineNode()
{
    AddOutputProperty("image", PropertyType::Image);

    AddConfig<ConfigurableType::Vec2Int>("size", glm::ivec2{512.0f, 512.0f});
    AddConfig<ConfigurableType::Format>("format", Gfx::ImageFormat::R8G8B8A8_UNorm);
    AddConfig<ConfigurableType::Int>("mip level", int{1});
}

Gfx::Image* ImageNode::GetImage()
{
    return (Gfx::Image*)imageNode->GetPass()->GetResourceRef(0)->GetResource();
}
char ImageNode::_reg = NodeBlueprintRegisteration::Register<ImageNode>("Image");
} // namespace Engine::FrameGraph
