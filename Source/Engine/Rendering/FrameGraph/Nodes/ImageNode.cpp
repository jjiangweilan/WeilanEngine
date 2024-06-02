#include "ImageNode.hpp"

namespace Rendering::FrameGraph
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

void ImageNode::Compile() {}

void ImageNode::Execute(Gfx::CommandBuffer& cmd, RenderingData& renderingData)
{
    const auto& screenSize = renderingData.screenSize;
    if (size->x == 0)
    {
        desc.SetWidth(screenSize.x);
    }
    else if (size->x < 0)
    {
        desc.SetWidth(screenSize.x * -size->x);
    }
    else
        desc.SetWidth(size->x);
    if (size->y == 0)
    {
        desc.SetHeight(screenSize.y);
    }
    else if (size->y < 0)
    {
        desc.SetHeight(screenSize.y * -size->y);
    }
    else
        desc.SetHeight(size->y);
    desc.SetFormat(*format);
    cmd.AllocateAttachment(id, desc);
    output.attachment->SetValue(AttachmentProperty{id, desc});
}

void ImageNode::DefineNode()
{
    output.attachment = AddOutputProperty("image", PropertyType::Attachment);

    AddConfig<ConfigurableType::Vec2>("size", glm::vec2{512.0f, 512.0f});
    AddConfig<ConfigurableType::Format>("format", Gfx::ImageFormat::R8G8B8A8_UNorm);
    AddConfig<ConfigurableType::Int>("mip level", int{1});

    size = GetConfigurablePtr<glm::vec2>("size");
    format = GetConfigurablePtr<Gfx::ImageFormat>("format");
}

const Gfx::RG::ImageIdentifier& ImageNode::GetImage()
{
    return id;
}
char ImageNode::_reg = NodeBlueprintRegisteration::Register<ImageNode>("Image");
} // namespace Rendering::FrameGraph
