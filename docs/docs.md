# WeilanEngine Coding Document

## GfxDriver
a very thin wrap of Graphics APIs, it's basically des

## Rendering

### Render Graph
A RenderGraph automatically manages the resource dependency (inserting GPUBarrier) and creates necessary resources.

* Create a graph
A RenderGraph is created without any parameter
```c++
auto graph = RenderGraph();
```

* Declare a basic RenderNode
you use graph's `AddNode` api to add a RenderNode into the graph. The node will create a RenderPass inside it.
`.handle` is any integer specified by user, no duplication is allowed. It's used in RenderPass declaration.
The example code shows to how create a image that's managed by the graph, and declare a RenderPass that use the image as color attachment
```c++
// graph will creates "opaque color" that later user can refer to
auto renderEditor = graph->AddNode(
    [&](Gfx::CommandBuffer& cmd, Gfx::RenderPass& pass, const RenderGraph::ResourceRefs& res)
    { this->RenderEditor(cmd, pass, res); }, // Execution function
    { // Resource declaration
        {
            .name = "opaque color",
            .handle = 0,
            .type = ResourceType::Image,
            .accessFlags = Gfx::AccessMask::Color_Attachment_Write,
            .stageFlags = Gfx::PipelineStage::Color_Attachment_Output,
            .imageUsagesFlags = Gfx::ImageUsage::ColorAttachment,
            .imageLayout = Gfx::ImageLayout::Color_Attachment,
            .imageCreateInfo =
                {
                    .width = width,
                    .height = height,
                    .format = Gfx::ImageFormat::R16G16B16A16_SFloat,
                    .multiSampling = Gfx::MultiSampling::Sample_Count_1,
                    .mipLevels = 1,
                    .isCubemap = false,
                },
        },
    },
    { // RenderPass declaration
        {
            .colors =
                {
                    {
                        .handle = 0, // refers to Resource "opaque color"
                        .loadOp = Gfx::AttachmentLoadOperation::Load,
                        .storeOp = Gfx::AttachmentStoreOperation::Store,
                    },
                },
        },
    }
);
```

# Declare a RenderNode that uses external image/buffer
There is `externalImage` and `externalBuffer` field in resource declaration field, when you want to introduce an external image or buffer you can
fill them out without filling the createInfo. Node for these external resource, resource usage field won't apply. User should introduce the external resource
once, later node that uses these resource generally should refer them through linking nodes together

```c++
auto blitToFinal = AddNode(
    [](Gfx::CommandBuffer& cmd, auto& pass, const ResourceRefs& res)
    {
        Gfx::Image* src = (Gfx::Image*)res.at(1)->GetResource();
        Gfx::Image* dst = (Gfx::Image*)res.at(0)->GetResource();
        cmd.Blit(src, dst);
    },
    {
        {
            .name = "external Image",
            .handle = 0,
            .type = RenderGraph::ResourceType::Image,
            .accessFlags = Gfx::AccessMask::Transfer_Write,
            .stageFlags = Gfx::PipelineStage::Transfer,
            .imageLayout = Gfx::ImageLayout::Transfer_Dst,
            .externalImage = &config.finalImage,
        },
        {
            .name = "src",
            .handle = 1,
            .type = RenderGraph::ResourceType::Image,
            .accessFlags = Gfx::AccessMask::Transfer_Read,
            .stageFlags = Gfx::PipelineStage::Transfer,
            .imageUsagesFlags = Gfx::ImageUsage::TransferSrc,
            .imageLayout = Gfx::ImageLayout::Transfer_Src,
        },
    },
    {}
);
```

# Use a different ImageView for Gfx::RenderPass
in RenderPass declaration there is an optional field called imageView. When not specified RenderGraph will use the default imageView for the image.
Node: reading and writing to the same image with different subresource is not currently supported (like for Hiz generation). It requires per subresource region
dependency tracking which is not implemented

```c++
struct Attachment
{
    ResourceHandle handle;
    std::optional<ImageView> imageView;
    Gfx::MultiSampling multiSampling = Gfx::MultiSampling::Sample_Count_1;
    Gfx::AttachmentLoadOperation loadOp = Gfx::AttachmentLoadOperation::Clear;
    Gfx::AttachmentStoreOperation storeOp = Gfx::AttachmentStoreOperation::Store;
    Gfx::AttachmentLoadOperation stencilLoadOp = Gfx::AttachmentLoadOperation::DontCare;
    Gfx::AttachmentStoreOperation stencilStoreOp = Gfx::AttachmentStoreOperation::DontCare;
};
```

### ImmediateGfx
ImmediateGfx introduces convinient APIs to interact with GfxDriver.
