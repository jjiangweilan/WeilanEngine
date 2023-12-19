#pragma once
#include "ImageNode.hpp"

namespace FrameGraph
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

    imageNode =
        graph.AddNode(GetCustomName())
            .AllocateRT(GetCustomName(), 0, size.x, size.y, format, mipLevel, Gfx::MultiSampling::Sample_Count_1)
            .Finish();

    // imageNode = graph.AddNode(
    //     [](Gfx::CommandBuffer&, Gfx::RenderPass&, const RenderGraph::ResourceRefs&) {},
    //     {{.name = GetCustomName(),
    //       .handle = 0,
    //       .type = RenderGraph::ResourceType::Image,
    //       .accessFlags = Gfx::AccessMask::None,
    //       .stageFlags = Gfx::PipelineStage::Top_Of_Pipe,
    //       .imageUsagesFlags = Gfx::ImageUsage::Texture | Gfx::ImageUsage::TransferSrc | Gfx::ImageUsage::TransferDst,
    //       .imageLayout = Gfx::IsDepthStencilFormat(format) ? Gfx::ImageLayout::Depth_Stencil_Attachment
    //                                                        : Gfx::ImageLayout::Color_Attachment,
    //       .imageCreateInfo =
    //           {
    //               .width = (uint32_t)size.x,
    //               .height = (uint32_t)size.y,
    //               .format = format,
    //               .multiSampling = Gfx::MultiSampling::Sample_Count_1,
    //               .mipLevels = (uint32_t)mipLevel,
    //               .isCubemap = false,
    //           }}},
    //     {}
    // );
    // imageNode->SetName(GetCustomName());

    return {
        Resource(ResourceTag::RenderGraphLink{}, outputPropertyIDs["image"], imageNode, 0),
        Resource(ResourceTag::Float{}, outputPropertyIDs["width"], size.x),
        Resource(ResourceTag::Float{}, outputPropertyIDs["width"], size.y)};
};

void ImageNode::DefineNode()
{
    AddOutputProperty("image", PropertyType::RenderGraphLink);

    AddConfig<ConfigurableType::Vec2Int>("size", glm::ivec2{512.0f, 512.0f});
    AddConfig<ConfigurableType::Format>("format", Gfx::ImageFormat::R8G8B8A8_UNorm);
    AddConfig<ConfigurableType::Int>("mip level", int{1});
}

Gfx::Image* ImageNode::GetImage()
{
    return (Gfx::Image*)imageNode->GetPass()->GetResourceRef(0)->GetResource();
}
char ImageNode::_reg = NodeBlueprintRegisteration::Register<ImageNode>("Image");
} // namespace FrameGraph
