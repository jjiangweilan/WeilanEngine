#include "ImmediateGfx.hpp"

namespace Engine
{
void ImmediateGfx::RenderToImage(
    Gfx::Image& image,
    Gfx::ImageSubresourceRange subresourceRanges,
    Gfx::ShaderProgram& shader,
    const Gfx::ShaderConfig& config,
    Gfx::ShaderResource& resource,
    Gfx::ImageLayout finalLayout
)
{
    RenderGraph::Graph graph;
    auto n0 = graph.AddNode(
        [&shader, &resource, &config, &image](Gfx::CommandBuffer& cmd, auto& pass, auto& res)
        {
            Gfx::ClearValue clear;
            clear.color = {{0, 0, 0, 0}};
            Rect2D scissor{{0, 0}, {image.GetDescription().width, image.GetDescription().height}};
            cmd.SetScissor(0, 1, &scissor);
            Gfx::Viewport viewport{
                .x = 0,
                .y = 0,
                .width = (float)scissor.extent.width,
                .height = (float)scissor.extent.height,
                .minDepth = 0,
                .maxDepth = 1,
            };
            cmd.SetViewport(viewport);
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
            .stageFlags = Gfx::PipelineStage::Color_Attachment_Output,
            .imageUsagesFlags = Gfx::ImageUsage::ColorAttachment,
            .imageLayout = Gfx::ImageLayout::Color_Attachment,
            .externalImage = &image,
        }},
        {{.colors = {{
              .handle = 0,
              .imageView = {{.imageViewType = Gfx::ImageViewType::Image_2D, .subresourceRange = subresourceRanges}},
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

std::unique_ptr<Gfx::Buffer> ImmediateGfx::CopyImageToBuffer(Gfx::Image& image)
{
    uint32_t bufferSize = image.GetDescription().GetByteSize();
    std::unique_ptr<Gfx::Buffer> buffer = GetGfxDriver()->CreateBuffer({
        .usages = Gfx::BufferUsage::Transfer_Dst,
        .size = bufferSize,
        .visibleInCPU = true,
    });
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
                        .layerCount = image.GetDescription().GetLayer(),
                    },
                .offset = {0, 0, 0},
                .extend = {image.GetDescription().width, image.GetDescription().height, 1},
            }

            };
            cmd.CopyImageToBuffer(&image, buffer.get(), copy);
        }
    );

    return buffer;
}
} // namespace Engine
