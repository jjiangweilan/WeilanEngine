#include "ImmediateGfx.hpp"

namespace Engine
{
void ImmediateGfx::RenderToImage(
    Gfx::Image& image,
    Gfx::ImageView& imageView,
    Gfx::ShaderProgram& shader,
    Gfx::ShaderConfig& config,
    Gfx::ShaderResource& resource,
    Gfx::ImageLayout finalLayout
)
{
    if (image.GetDescription().isCubemap)
    {
        return;
    }

    RenderGraph::Graph graph;
    auto n0 = graph.AddNode(
        [&shader, &resource, &config](Gfx::CommandBuffer& cmd, auto& pass, auto& res)
        {
            Gfx::ClearValue clear;
            clear.color = {0, 0, 0, 0};
            cmd.BeginRenderPass(pass, {clear});
            cmd.BindResource(&resource);
            cmd.BindShaderProgram(&shader, config);
            cmd.Draw(3, 1, 0, 0);
            cmd.EndRenderPass();
        },
        {{
            .name = "image",
            .handle = 0,
            .type = RenderGraph::ResourceType::Image,
            .accessFlags = Gfx::AccessMask::Color_Attachment_Write,
            .stageFlags = Gfx::PipelineStage::Fragment_Shader,
            .imageUsagesFlags = Gfx::ImageUsage::ColorAttachment,
            .imageLayout = Gfx::ImageLayout::Color_Attachment,
            .externalImage = &image,
        }},
        {{.colors = {{
              .handle = 0,
              .imageView = {{
                  .imageViewType = Gfx::ImageViewType::Image_2D,
                  .subresourceRange =
                      {
                          .aspectMask = Gfx::ImageAspectFlags::Color,
                          .baseMipLevel = 0,
                          .levelCount = 1,
                          .baseArrayLayer = 0,
                          .layerCount = 1,
                      },
              }},
              .loadOp = Gfx::AttachmentLoadOperation::Clear,
              .storeOp = Gfx::AttachmentStoreOperation::Store,
          }}}}
    );

    auto n1 = graph.AddNode(
        [&shader, &resource, &config](Gfx::CommandBuffer& cmd, auto& pass, auto& res) {},
        {{
            .name = "image",
            .handle = 0,
            .type = RenderGraph::ResourceType::Image,
            .accessFlags = Gfx::AccessMask::Memory_Read | Gfx::AccessMask::Memory_Write,
            .stageFlags = Gfx::PipelineStage::Top_Of_Pipe,
            .imageUsagesFlags = Gfx::ImageUsage::TransferDst,
            .imageLayout = finalLayout,
        }},
        {}
    );
    graph.Connect(n0, 0, n1, 0);
    graph.Process();

    OnetimeSubmit([&graph](Gfx::CommandBuffer& cmd) { graph.Execute(cmd); });
}

void ImmediateGfx::CopyImageToBuffer(Gfx::Image& image, Gfx::Buffer& buffer)
{
    OnetimeSubmit(
        [&image, &buffer](Gfx::CommandBuffer& cmd)
        {
            Gfx::BufferImageCopyRegion copy[1] = {{
                .srcOffset = 0,
                .layers =
                    {
                        .aspectMask = Gfx::ImageAspectFlags::Color,
                        .mipLevel = 0,
                        .baseArrayLayer = 0,
                        .layerCount = Gfx::Remaining_Array_Layers,
                    },
                .offset = {0, 0, 0},
                .extend = {image.GetDescription().width, image.GetDescription().height, 1},
            }

            };
            cmd.CopyImageToBuffer(&image, &buffer, copy);
        }
    );
}
} // namespace Engine
