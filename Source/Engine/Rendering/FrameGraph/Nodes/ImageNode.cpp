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

void ImageNode::Compile()
{
    glm::uvec2 size = GetConfigurableVal<glm::ivec2>("size");
    Gfx::ImageFormat format = GetConfigurableVal<Gfx::ImageFormat>("format");

    desc = {size.x, size.y, format};

    if (Gfx::IsColoFormat(format))
        image = GetGfxDriver()->CreateImage(
            Gfx::ImageDescription{size.x, size.y, format},
            Gfx::ImageUsage::ColorAttachment | Gfx::ImageUsage::Texture
        );
    else
        image = GetGfxDriver()->CreateImage(
            Gfx::ImageDescription{size.x, size.y, format},
            Gfx::ImageUsage::DepthStencilAttachment | Gfx::ImageUsage::Texture
        );

    image->SetName(fmt::format("image node {}", GetName()));
    id = *image;
}

void ImageNode::Execute(Gfx::CommandBuffer& cmd, RenderingData& renderingData)
{
    output.attachment->SetValue(AttachmentProperty{id, desc});
}

void ImageNode::DefineNode()
{
    output.attachment = AddOutputProperty("image", PropertyType::Attachment);

    AddConfig<ConfigurableType::Vec2Int>("size", glm::ivec2{512.0f, 512.0f});
    AddConfig<ConfigurableType::Format>("format", Gfx::ImageFormat::R8G8B8A8_UNorm);
    AddConfig<ConfigurableType::Int>("mip level", int{1});
}

Gfx::Image* ImageNode::GetImage()
{
    return image.get();
}
char ImageNode::_reg = NodeBlueprintRegisteration::Register<ImageNode>("Image");
} // namespace FrameGraph
