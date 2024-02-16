#include "Graph.hpp"
#include "Errors.hpp"

#include <algorithm>
#include <spdlog/spdlog.h>

namespace RenderGraph
{
RenderNode::RenderNode(std::unique_ptr<RenderPass>&& pass_, const std::string& debugDesc)
    : name(debugDesc), pass(std::move(pass_))
{}

ResourceHandle StrToHandle(const std::string& str)
{
    ResourceHandle rlt = std::hash<std::string>()(str);
    return rlt;
}

void Graph::Process(RenderNode* presentNode, ResourceHandle resourceHandle)
{
    Process();

    if (presentNode != nullptr)
    {
        if (presentNode->pass->GetResourceRef(resourceHandle)->GetResource() != GetGfxDriver()->GetSwapChainImage())
        {
            throw std::logic_error("present port needs to be swapchain image");
        }
    }
}

Graph::~Graph() {}

bool Graph::Connect(RenderNode* src, ResourceHandle srcHandle, RenderNode* dst, ResourceHandle dstHandle)
{
    if (src == nullptr || dst == nullptr)
    {
        SPDLOG_ERROR("Connecting nullptr RenderNode");
        return false;
    }

    bool srcHasHandleDescription = src->pass->HasResourceDescription(srcHandle);
    bool dstHasHandleDescription = dst->pass->HasResourceDescription(dstHandle);
    if (src && dst && srcHasHandleDescription && dstHasHandleDescription &&
        (src->pass->GetResourceDescription(srcHandle).type == dst->pass->GetResourceDescription(dstHandle).type))
    {
        src->outputPorts.emplace_back(new RenderNode::Port({.handle = srcHandle, .parent = src}));
        dst->inputPorts.emplace_back(new RenderNode::Port({.handle = dstHandle, .parent = dst}));
        src->outputPorts.back()->connected = dst->inputPorts.back().get();
        dst->inputPorts.back()->connected = src->outputPorts.back().get();
    }
    else
    {
        SPDLOG_ERROR("Connecting RenderNodes failed");
        return false;
    }

    return true;
}

void Graph::Process()
{
    // clean up
    resourceOwners.clear();
    resourcePool.Clear();
    sortedNodes.clear();

    std::vector<RenderNode*> sortedNodes(nodes.size());
    std::transform(nodes.begin(), nodes.end(), sortedNodes.begin(), [](auto& n) { return n.get(); });

    // recalculate depth
    for (RenderNode* n : sortedNodes)
        n->depth = -1;
    for (RenderNode* n : sortedNodes)
        n->depth = GetDepthOfNode(n);

    // sort nodes
    std::sort(sortedNodes.begin(), sortedNodes.end(), [](RenderNode* l, RenderNode* r) { return l->depth < r->depth; });
    for (int i = 0; i < sortedNodes.size(); ++i)
        sortedNodes[i]->sortIndex = i;

    // preprocess
    for (RenderNode* n : sortedNodes)
    {
        Preprocess(n);
    }

    for (auto& resourceOwner : resourceOwners)
    {
        resourceOwner->Finalize(resourcePool);
    }

    for (RenderNode* n : sortedNodes)
    {
        n->pass->Finalize();
    }

    // copy
    this->sortedNodes = sortedNodes;
}

void Graph::ResourceOwner::Finalize(ResourcePool& pool)
{
    if (request.type == ResourceType::Image)
    {
        if (request.externalImage == nullptr)
        {
            Gfx::Image* image = pool.CreateImage(request.imageCreateInfo, request.imageUsagesFlags);
            image->SetName(request.name);
            resourceRef.SetResource(image);
        }
        else
        {
            resourceRef.SetResource(request.externalImage);
        }
    }
    else if (request.type == ResourceType::Buffer)
    {
        if (request.externalBuffer == nullptr)
        {
            Gfx::Buffer* buf = pool.CreateBuffer(request.bufferCreateInfo);
            buf->SetDebugName(request.name.c_str());
            resourceRef.SetResource(buf);
        }
        else
        {
            resourceRef.SetResource(request.externalBuffer);
        }
    }
}

Graph::ResourceOwner* Graph::CreateResourceOwner(const RenderPass::ResourceDescription& request)
{
    auto owner = std::make_unique<ResourceOwner>(request);
    resourceOwners.push_back(std::move(owner));
    return resourceOwners.back().get();
}

int Graph::GetDepthOfNode(RenderNode* node)
{
    if (node)
    {
        if (node->depth != -1)
            return node->depth;
        else
        {
            int depth = 1;
            for (auto& port : node->inputPorts)
            {
                RenderNode::Port* connected = port->connected;
                if (connected)
                {
                    depth += GetDepthOfNode(connected->parent);
                }
            }

            return depth;
        }
    }

    return 0;
}

void Graph::Preprocess(RenderNode* n)
{
    // create resource owner for creation request and external resources
    auto creationRequests = n->pass->GetResourceCreationRequests();
    auto externalResources = n->pass->GetExternalResources();
    std::vector<ResourceHandle> resourceHandles;
    resourceHandles.assign(creationRequests.begin(), creationRequests.end());
    resourceHandles.insert(resourceHandles.end(), externalResources.begin(), externalResources.end());

    for (auto handle : resourceHandles)
    {
        auto& request = n->pass->GetResourceDescription(handle);
        ResourceOwner* owner = CreateResourceOwner(request);
        n->pass->SetResourceRef(request.handle, &owner->resourceRef);
        owner->used.push_back({n->sortIndex, handle});
    }

    // set resource from connected
    for (auto& inPort : n->inputPorts)
    {
        RenderNode::Port* connected = inPort->connected;
        if (connected == nullptr)
            throw Errrors::GraphCompile("A node has unconnected port");

        auto resRef = connected->parent->pass->GetResourceRef(connected->handle);
        ResourceOwner* owner = (ResourceOwner*)resRef->GetOwner();
        owner->used.push_back({inPort->parent->sortIndex, inPort->handle});

        auto& fromDesc = n->pass->GetResourceDescription(inPort->handle);
        owner->request.imageUsagesFlags |= fromDesc.imageUsagesFlags;
        owner->request.bufferCreateInfo.usages |= fromDesc.bufferCreateInfo.usages;

        inPort->parent->pass->SetResourceRef(inPort->handle, resRef);
    }
}

void Graph::Compile() {}

Gfx::Buffer* Graph::ResourcePool::CreateBuffer(const Gfx::Buffer::CreateInfo& createInfo)
{
    buffers.push_back(std::move(GetGfxDriver()->CreateBuffer(createInfo)));

    return buffers.back().get();
}
Gfx::Image* Graph::ResourcePool::CreateImage(const Gfx::ImageDescription& imageDesc, Gfx::ImageUsageFlags usages)
{
    images.push_back(std::move(GetGfxDriver()->CreateImage(imageDesc, usages)));

    return images.back().get();
}

void Graph::ResourcePool::ReleaseBuffer(Gfx::Buffer* handle)
{
    auto iter = std::find_if(
        buffers.begin(),
        buffers.end(),
        [handle](std::unique_ptr<Gfx::Buffer>& p) { return p.get() == handle; }
    );

    if (iter != buffers.end())
    {
        buffers.erase(iter);
    }
}

void Graph::ResourcePool::ReleaseImage(Gfx::Image* handle)
{
    auto iter = std::find_if(
        images.begin(),
        images.end(),
        [handle](std::unique_ptr<Gfx::Image>& p) { return p.get() == handle; }
    );

    if (iter != images.end())
    {
        images.erase(iter);
    }
}

void Graph::Execute(Gfx::CommandBuffer& cmd)
{
    for (auto n : sortedNodes)
    {
        n->pass->Execute(cmd);
    }
}

void Graph::Clear()
{
    nodes.clear();
    sortedNodes.clear();
    resourceOwners.clear();
    resourcePool.Clear();
}

NodeBuilder& NodeBuilder::InputTexture(
    std::string_view name,
    ResourceHandle handle,
    Gfx::PipelineStage stageFlags,
    Gfx::Image* externalImage,
    std::optional<Gfx::ImageSubresourceRange> imageSubresourceRange
)
{
    if (descs.find(handle) != descs.end())
    {
        SPDLOG_WARN("duplicate node input handle");
        return *this;
    }

    descs[handle] = RenderPass::ResourceDescription{
        .name = std::string(name),
        .handle = handle,
        .type = ResourceType::Image,

        .accessFlags = Gfx::AccessMask::Shader_Read,
        .stageFlags = stageFlags,
        .imageUsagesFlags = Gfx::ImageUsage::Texture,
        .imageLayout = Gfx::ImageLayout::Shader_Read_Only,
        .externalImage = externalImage,
        .imageSubresourceRange = imageSubresourceRange,
    };

    return *this;
}

NodeBuilder& NodeBuilder::InputRT(
    std::string_view name,
    ResourceHandle handle,
    Gfx::Image* externalImage,
    // used when you want to set a barrier to part of the image
    std::optional<Gfx::ImageSubresourceRange> imageSubresourceRange
)
{
    descs[handle] = RenderPass::ResourceDescription{
        .name = std::string(name),
        .handle = handle,
        .type = ResourceType::Image,
        .accessFlags = Gfx::AccessMask::None,       // access flag is filled later
        .stageFlags = Gfx::PipelineStage::None,     // usage is filled later
        .imageUsagesFlags = Gfx::ImageUsage::None,  // changed later
        .imageLayout = Gfx::ImageLayout::Undefined, // changed layer
        .externalImage = externalImage,
        .imageSubresourceRange = imageSubresourceRange,
    };

    return *this;
}

NodeBuilder& NodeBuilder::InputBuffer(
    std::string name, ResourceHandle handle, Gfx::PipelineStage stageFlags, Gfx::Buffer* externalBuffer
)
{
    if (descs.find(handle) != descs.end())
    {
        SPDLOG_WARN("duplicate node input handle");
        return *this;
    }

    descs[handle] = RenderPass::ResourceDescription{
        .name = name,
        .handle = handle,
        .type = ResourceType::Buffer,
        .accessFlags = Gfx::AccessMask::Shader_Read,
        .stageFlags = stageFlags,
        .externalBuffer = externalBuffer,
    };

    return *this;
}

NodeBuilder& NodeBuilder::AllocateRT(
    std::string_view name,
    ResourceHandle handle,
    uint32_t width,
    uint32_t height,
    Gfx::ImageFormat format,
    uint32_t mipLevel,
    Gfx::MultiSampling multiSampling,
    Gfx::ImageUsageFlags extraUsages
)
{
    if (descs.find(handle) != descs.end())
    {
        SPDLOG_WARN("duplicate node input handle");
        return *this;
    }

    descs[handle] = RenderPass::ResourceDescription{
        .name = std::string(name),
        .handle = handle,
        .type = ResourceType::Image,
        .accessFlags = Gfx::AccessMask::None,          // access flag is filled later
        .stageFlags = Gfx::PipelineStage::Top_Of_Pipe, // usage is filled later
        .imageUsagesFlags = extraUsages |
                            (Gfx::IsDepthStencilFormat(format) ? Gfx::ImageUsage::DepthStencilAttachment
                                                               : Gfx::ImageUsage::ColorAttachment) |
                            Gfx::ImageUsage::Texture,
        .imageLayout = Gfx::IsDepthStencilFormat(format)
                           ? Gfx::ImageLayout::Depth_Stencil_Attachment
                           : Gfx::ImageLayout::Color_Attachment, // changed later in AddColor

        .imageCreateInfo = Gfx::ImageDescription(width, height, 1, format, multiSampling, mipLevel, false),

        .externalImage = nullptr,
    };

    return *this;
}

NodeBuilder& NodeBuilder::NextSubpass()
{
    // not implemented
    return *this;
}
NodeBuilder& NodeBuilder::AddColor(
    ResourceHandle handle,
    bool blend,
    Gfx::AttachmentLoadOperation loadOp,
    Gfx::AttachmentStoreOperation storeOp,
    uint32_t mip,
    uint32_t arrayLayer
)
{
    auto iter = descs.find(handle);
    if (iter == descs.end())
    {
        SPDLOG_ERROR("No such resources");
        return *this;
    }

    if (subpasses.empty())
    {
        subpasses.push_back(RenderPass::Subpass{});
    }

    auto& desc = iter->second;
    desc.stageFlags = Gfx::PipelineStage::Color_Attachment_Output;
    desc.accessFlags = (storeOp == Gfx::AttachmentStoreOperation::Store ? Gfx::AccessMask::Color_Attachment_Write
                                                                        : Gfx::AccessMask::None) |
                       (blend ? Gfx::AccessMask::Color_Attachment_Read : Gfx::AccessMask::None);
    desc.imageUsagesFlags |= Gfx::ImageUsage::ColorAttachment;
    desc.imageLayout = Gfx::ImageLayout::Color_Attachment;

    RenderPass::Attachment att{
        .handle = handle,
        .imageView =
            RenderPass::ImageView{
                .imageViewType = Gfx::ImageViewType::Image_2D,
                .subresourceRange =
                    {
                        .aspectMask = Gfx::ImageAspect::Color,
                        .baseMipLevel = mip,
                        .levelCount = 1,
                        .baseArrayLayer = arrayLayer,
                        .layerCount = 1,
                    },
            },
        .multiSampling = desc.imageCreateInfo.multiSampling,
        .loadOp = loadOp,
        .storeOp = storeOp,
        .stencilLoadOp = Gfx::AttachmentLoadOperation::DontCare,
        .stencilStoreOp = Gfx::AttachmentStoreOperation::DontCare,
    };

    subpasses[subpassesIndex].colors.push_back(att);

    return *this;
}

NodeBuilder& NodeBuilder::AddDepthStencil(
    ResourceHandle handle,
    Gfx::AttachmentLoadOperation loadOp,
    Gfx::AttachmentStoreOperation storeOp,
    Gfx::AttachmentLoadOperation stencilLoadOp,
    Gfx::AttachmentStoreOperation stencilStoreOp,
    uint32_t mip,
    uint32_t arrayLayer
)
{
    auto iter = descs.find(handle);
    if (iter == descs.end())
    {
        SPDLOG_ERROR("No such resources");
        return *this;
    }

    if (subpasses.empty())
    {
        subpasses.push_back(RenderPass::Subpass{});
    }

    auto& desc = iter->second;
    desc.stageFlags = Gfx::PipelineStage::Early_Fragment_Tests | Gfx::PipelineStage::Late_Fragment_Tests;
    desc.accessFlags =
        (storeOp == Gfx::AttachmentStoreOperation::Store ? Gfx::AccessMask::Depth_Stencil_Attachment_Write
                                                         : Gfx::AccessMask::None) |
        Gfx::AccessMask::Depth_Stencil_Attachment_Read;
    desc.imageUsagesFlags |= Gfx::ImageUsage::DepthStencilAttachment;
    desc.imageLayout = Gfx::ImageLayout::Depth_Stencil_Attachment;

    RenderPass::Attachment att{
        .handle = handle,
        .imageView =
            RenderPass::ImageView{
                .imageViewType = Gfx::ImageViewType::Image_2D,
                .subresourceRange =
                    {
                        .aspectMask = Gfx::ImageAspect::Depth |
                                      (Gfx::HasStencil(desc.imageCreateInfo.format) ? Gfx::ImageAspect::Stencil
                                                                                    : Gfx::ImageAspect::None),
                        .baseMipLevel = mip,
                        .levelCount = 1,
                        .baseArrayLayer = arrayLayer,
                        .layerCount = 1,
                    },
            },
        .multiSampling = desc.imageCreateInfo.multiSampling,
        .loadOp = loadOp,
        .storeOp = storeOp,
        .stencilLoadOp = Gfx::AttachmentLoadOperation::DontCare,
        .stencilStoreOp = Gfx::AttachmentStoreOperation::DontCare,
    };

    subpasses[subpassesIndex].depth = att;

    return *this;
}

NodeBuilder& NodeBuilder::AllocateBuffer(std::string_view name, ResourceHandle handle, size_t size)
{
    // not implemented
    return *this;
}

RenderNode* NodeBuilder::Finish()
{
    finished = true;
    std::vector<RenderPass::ResourceDescription> resourceDescriptions;
    resourceDescriptions.reserve(descs.size());
    for (auto& iter : descs)
    {
        resourceDescriptions.push_back(iter.second);
    }
    auto node = graph->AddNode(execFunc, resourceDescriptions, subpasses);
    node->SetName(name);
    return node;
}

} // namespace RenderGraph
