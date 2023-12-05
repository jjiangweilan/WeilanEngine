#include "Graph.hpp"
#include "Errors.hpp"

#include <algorithm>
#include <spdlog/spdlog.h>

namespace Engine::RenderGraph
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
    RenderNode* present = AddNode(
        [](auto& cmd, auto& renderPass, const auto& bufs) {},
        {{
            .name = "swapchainImage",
            .handle = 0,
            .type = ResourceType::Image,
            .accessFlags = Gfx::AccessMask::None,
            .stageFlags = Gfx::PipelineStage::Bottom_Of_Pipe,
            .imageUsagesFlags = Gfx::ImageUsage::TransferDst,
            .imageLayout = Gfx::ImageLayout::Present_Src_Khr,
        }},
        {}
    );

    Connect(presentNode, resourceHandle, present, 0);

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

void Graph::Connect(RenderNode* src, ResourceHandle srcHandle, RenderNode* dst, ResourceHandle dstHandle)
{
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
        throw std::logic_error("Graph Can't connect two nodes");
    }
}

void Graph::Process()
{
    // clean up
    resourceOwners.clear();
    resourcePool.Clear();
    barrierNodes.clear();
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

    // insert resources barriers
    std::unordered_map<SortIndex, std::vector<Gfx::GPUBarrier>> barriers;
    std::vector<Gfx::GPUBarrier> initialLayoutTransfers;

    for (std::unique_ptr<ResourceOwner>& r : resourceOwners)
    {
        if (r->used.empty())
            assert("Shouldn't be empty, if empty should be culled");

        auto lastUsed = r->used.back();
        Gfx::ImageLayout currentLayout =
            sortedNodes[lastUsed.first]->pass->GetResourceDescription(lastUsed.second).imageLayout;

        if (r->resourceRef.IsType(ResourceType::Image))
        {
            auto image = (Gfx::Image*)r->resourceRef.GetResource();
            Gfx::GPUBarrier initialLayoutTransfer{
                .image = image,
                .srcStageMask = Gfx::PipelineStage::All_Commands,
                .dstStageMask = Gfx::PipelineStage::All_Commands,
                .srcAccessMask = Gfx::AccessMask::Memory_Read | Gfx::AccessMask::Memory_Write,
                .dstAccessMask = Gfx::AccessMask::Memory_Read | Gfx::AccessMask::Memory_Write,
                .imageInfo =
                    {
                        .srcQueueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED,
                        .dstQueueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED,
                        .oldLayout = Gfx::ImageLayout::Undefined,
                        .newLayout = currentLayout,
                        .subresourceRange = image->GetSubresourceRange(),
                    },
            };
            initialLayoutTransfers.push_back(initialLayoutTransfer);
        }

        using UsedIndex = int;
        std::vector<UsedIndex> readAccess;
        std::vector<UsedIndex> writeAccess;
        for (UsedIndex usedIndex = 0; usedIndex < r->used.size(); ++usedIndex)
        {
            auto p = r->used[usedIndex];
            SortIndex sortIndex = p.first;
            ResourceHandle handle = p.second;
            auto& desc = sortedNodes[sortIndex]->pass->GetResourceDescription(handle);
            if (r->resourceRef.IsType(ResourceType::Image))
            {
                Gfx::Image* image = (Gfx::Image*)r->resourceRef.GetResource();
                std::vector<Gfx::ImageSubresourceRange> remainingRange{
                    desc.imageSubresourceRange.has_value() ? desc.imageSubresourceRange.value()
                                                           : image->GetSubresourceRange()};
                std::vector<Gfx::ImageSubresourceRange> remainingRangeSwap{};

                for (UsedIndex preUsed = usedIndex; preUsed >= 0; preUsed--)
                {
                    if (preUsed != 0)
                        preUsed = preUsed - 1;

                    SortIndex preUsedSortIndex = r->used[preUsed].first;
                    ResourceHandle preUsedWriteHandle = r->used[preUsed].second;
                    auto preUsedDesc = sortedNodes[preUsedSortIndex]->pass->GetResourceDescription(preUsedWriteHandle);
                    auto preUsedRange = preUsedDesc.imageSubresourceRange.has_value()
                                            ? preUsedDesc.imageSubresourceRange.value()
                                            : image->GetSubresourceRange();

                    if (preUsed == 0)
                    {
                        preUsedDesc.imageLayout = Gfx::ImageLayout::Dynamic;
                    }

                    for (auto& currentRange : remainingRange)
                    {
                        if (currentRange.Overlaps(preUsedRange))
                        {
                            Gfx::ImageSubresourceRange overlapping = currentRange.And(preUsedRange);
                            Gfx::PipelineStageFlags srcStages = Gfx::PipelineStage::None;
                            Gfx::AccessMaskFlags srcAccess = Gfx::AccessMask::None;

                            if (Gfx::HasWriteAccessMask(desc.accessFlags) ||
                                preUsedDesc.imageLayout != desc.imageLayout)
                            {
                                // write after write
                                if (Gfx::HasWriteAccessMask(preUsedDesc.accessFlags))
                                {
                                    srcAccess |= preUsedDesc.accessFlags;
                                    srcStages |= preUsedDesc.stageFlags;
                                }

                                // write after read
                                if (Gfx::HasReadAccessMask(preUsedDesc.accessFlags))
                                {
                                    srcAccess |= preUsedDesc.accessFlags;
                                    srcStages |= preUsedDesc.stageFlags;
                                }
                            }

                            if (Gfx::HasReadAccessMask(desc.accessFlags))
                            {
                                // read after write
                                if (Gfx::HasWriteAccessMask(preUsedDesc.accessFlags))
                                {
                                    srcAccess |= preUsedDesc.accessFlags;
                                    srcStages |= preUsedDesc.stageFlags;
                                }
                            }

                            if (srcStages != Gfx::PipelineStage::None || srcAccess != Gfx::AccessMask::None ||
                                preUsedDesc.imageLayout != desc.imageLayout)
                            {
                                auto image = (Gfx::Image*)r->resourceRef.GetResource();
                                if (srcStages == Gfx::PipelineStage::None)
                                    srcStages = Gfx::PipelineStage::Top_Of_Pipe;
                                Gfx::GPUBarrier barrier{
                                    .image = image,
                                    .srcStageMask = srcStages,
                                    .dstStageMask = desc.stageFlags,
                                    .srcAccessMask = srcAccess,
                                    .dstAccessMask = desc.accessFlags,
                                    .imageInfo = {
                                        .srcQueueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED,
                                        .dstQueueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED,
                                        .oldLayout = preUsedDesc.imageLayout,
                                        .newLayout = desc.imageLayout,
                                        .subresourceRange = overlapping}};

                                barriers[sortIndex].push_back(barrier);
                            }

                            auto remainings = currentRange.Subtract(preUsedRange);
                            remainingRangeSwap.insert(remainingRangeSwap.end(), remainings.begin(), remainings.end());
                        }
                        else
                        {
                            remainingRangeSwap.push_back(currentRange);
                        }
                    }

                    std::swap(remainingRange, remainingRangeSwap);
                    remainingRangeSwap.clear();

                    // break
                    if (remainingRange.empty())
                        break;
                }
            }
            else if (r->resourceRef.IsType(ResourceType::Buffer))
            {
                Gfx::Buffer* buf = (Gfx::Buffer*)r->resourceRef.GetResource();

                Gfx::PipelineStageFlags srcStages = Gfx::PipelineStage::Top_Of_Pipe;
                Gfx::AccessMask srcAccessMask = Gfx::AccessMask::None;

                if (Gfx::HasWriteAccessMask(desc.accessFlags))
                {
                    // write after write
                    if (!writeAccess.empty())
                    {
                        UsedIndex i = writeAccess.back();
                        SortIndex srcWriteSortIndex = r->used[i].first;
                        ResourceHandle srcWriteHandle = r->used[i].second;
                        auto& srcWriteDesc =
                            sortedNodes[srcWriteSortIndex]->pass->GetResourceDescription(srcWriteHandle);
                        srcAccessMask |= srcWriteDesc.accessFlags;
                        srcStages |= srcWriteDesc.stageFlags;
                    }

                    // write after read
                    if (!readAccess.empty())
                    {
                        UsedIndex i = readAccess.back();
                        SortIndex srcReadSortIndex = r->used[i].first;
                        ResourceHandle srcReadHandle = r->used[i].second;
                        auto& srcReadDesc = sortedNodes[srcReadSortIndex]->pass->GetResourceDescription(srcReadHandle);
                        srcAccessMask |= srcReadDesc.accessFlags;
                        srcStages |= srcReadDesc.stageFlags;
                    }

                    writeAccess.push_back(usedIndex);
                }

                if (Gfx::HasReadAccessMask(desc.accessFlags))
                {
                    // read after write
                    if (!writeAccess.empty())
                    {
                        UsedIndex i = writeAccess.back();
                        SortIndex srcWriteSortIndex = r->used[i].first;
                        ResourceHandle srcWriteHandle = r->used[i].second;
                        auto& srcWriteDesc =
                            sortedNodes[srcWriteSortIndex]->pass->GetResourceDescription(srcWriteSortIndex);
                        srcAccessMask |= srcWriteDesc.accessFlags;
                        srcStages |= srcWriteDesc.stageFlags;
                    }

                    readAccess.push_back(usedIndex);
                }

                if (srcStages != Gfx::PipelineStage::Top_Of_Pipe || srcAccessMask != Gfx::AccessMask::None)
                {
                    Gfx::GPUBarrier barrier{
                        .buffer = (Gfx::Buffer*)r->resourceRef.GetResource(),
                        .srcStageMask = srcStages,
                        .dstStageMask = desc.stageFlags,
                        .srcAccessMask = srcAccessMask,
                        .dstAccessMask = desc.accessFlags,
                        .bufferInfo = {GFX_QUEUE_FAMILY_IGNORED, GFX_QUEUE_FAMILY_IGNORED, 0, GFX_WHOLE_SIZE},
                    };

                    barriers[sortIndex].push_back(barrier);
                }
            }
        }
    }

    // convert map barriers to vector barriers so that we can safely insert into sortedNodes
    std::vector<std::pair<SortIndex, std::vector<Gfx::GPUBarrier>>> vecBarriers;
    for (auto& b : barriers)
    {
        vecBarriers.emplace_back(b.first, b.second);
    }
    for (auto& b : vecBarriers)
    {
        SortIndex sortIndex = b.first;
        std::vector<Gfx::GPUBarrier>& gpuBarriers = b.second;

        std::unique_ptr<RenderPass> pass(new RenderPass(
            [gpuBarriers](Gfx::CommandBuffer& cmd, auto& b, auto& c)
            // cast to remove const
            { cmd.Barrier((Gfx::GPUBarrier*)gpuBarriers.data(), gpuBarriers.size()); },
            {},
            {}
        ));

        barrierNodes.push_back(std::make_unique<RenderNode>(std::move(pass), "Barrier Node"));

        sortedNodes.insert(sortedNodes.begin() + sortIndex, barrierNodes.back().get());

        for (auto& bb : vecBarriers)
        {
            if (bb.first >= sortIndex)
            {
                bb.first += 1;
            }
        }
    }

    // copy
    this->sortedNodes = sortedNodes;

    auto queue = GetGfxDriver()->GetQueue(QueueType::Main).Get();
    auto commandPool = GetGfxDriver()->CreateCommandPool({.queueFamilyIndex = queue->GetFamilyIndex()});
    std::unique_ptr<Gfx::CommandBuffer> cmd =
        commandPool->AllocateCommandBuffers(Gfx::CommandBufferType::Primary, 1)[0];

    cmd->Begin();
    cmd->Barrier(initialLayoutTransfers.data(), initialLayoutTransfers.size());
    cmd->End();

    Gfx::CommandBuffer* cmdBufs[] = {cmd.get()};
    GetGfxDriver()->QueueSubmit(queue, cmdBufs, {}, {}, {}, nullptr);
    GetGfxDriver()->WaitForIdle();
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
    barrierNodes.clear();
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
        .accessFlags = Gfx::AccessMask::None,                 // access flag is filled later
        .stageFlags = Gfx::PipelineStage::None,               // usage is filled later
        .imageUsagesFlags = Gfx::ImageUsage::ColorAttachment, // changed later
        .imageLayout = Gfx::ImageLayout::Shader_Read_Only,    // changed layer
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
    Gfx::MultiSampling multiSampling
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
        .accessFlags = Gfx::AccessMask::None,                 // access flag is filled later
        .stageFlags = Gfx::PipelineStage::None,               // usage is filled later
        .imageUsagesFlags = Gfx::ImageUsage::ColorAttachment, // changed later
        .imageLayout = Gfx::ImageLayout::Shader_Read_Only,    // changed layer
                                                              //
        .imageCreateInfo =
            {
                .width = width,
                .height = height,
                .format = format,
                .multiSampling = multiSampling,
                .mipLevels = mipLevel,
                .isCubemap = false,
            },

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
    desc.imageUsagesFlags = Gfx::ImageUsage::ColorAttachment;
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
    desc.imageUsagesFlags = Gfx::ImageUsage::DepthStencilAttachment;
    desc.imageLayout = Gfx::ImageLayout::Depth_Stencil_Attachment;

    RenderPass::Attachment att{
        .handle = handle,
        .imageView =
            RenderPass::ImageView{
                .imageViewType = Gfx::ImageViewType::Image_2D,
                .subresourceRange =
                    {
                        .aspectMask =
                            Gfx::ImageAspect::Depth | ((stencilLoadOp == Gfx::AttachmentLoadOperation::DontCare &&
                                                        stencilStoreOp == Gfx::AttachmentStoreOperation::DontCare)
                                                           ? Gfx::ImageAspect::None
                                                           : Gfx::ImageAspect::Stencil),
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

} // namespace Engine::RenderGraph
