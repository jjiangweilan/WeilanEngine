#include "Graph.hpp"
#include "Errors.hpp"

#include <algorithm>

namespace Engine::RenderGraph
{
RenderNode::RenderNode(std::unique_ptr<RenderPass>&& pass_, const std::string& debugDesc)
    : pass(std::move(pass_)), debugDesc(debugDesc)
{
    auto inputs = pass->GetResourceInputs();
    auto outputs = pass->GetResourceOutputs();
    for (int i = 0; i < inputs.size(); ++i)
    {
        inputPorts.emplace_back(this, inputs[i]);
    }
    for (int i = 0; i < outputs.size(); ++i)
    {
        outputPorts.emplace_back(this, outputs[i]);
    }
}

bool RenderNode::Connect(Port& src, Port& dst)
{
    RenderNode* srcNode = src.parent;
    RenderNode* dstNode = dst.parent;
    auto& srcDesc = srcNode->pass->GetResourceDescription(src.handle);
    auto& dstDesc = dstNode->pass->GetResourceDescription(dst.handle);

    if (srcDesc.type != dstDesc.type)
        return false;

    src.connected = &dst;
    dst.connected = &src;

    return true;
}

void Graph::Process(Port* presentPort)
{
    if (presentPort != nullptr)
    {
        if (presentPort->parent->pass->GetResourceDescription(presentPort->handle).externalImage !=
            GetGfxDriver()->GetSwapChainImageProxy().Get())
        {
            throw std::logic_error("present port needs to be swapchain image");
        }

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
            {},
            {0},
            {},
            {}
        );

        RenderNode::Connect(*presentPort, present->GetInputPorts()[0]);
    }

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
    std::unordered_map<SortIndex, std::vector<GPUBarrier>> barriers;
    std::vector<GPUBarrier> initialLayoutTransfers;

    for (std::unique_ptr<ResourceOwner>& r : resourceOwners)
    {
        if (r->used.empty())
            assert("Shouldn't be empty, if empty should be culled");

        auto lastUsed = r->used.back();
        Gfx::ImageLayout currentLayout =
            sortedNodes[lastUsed.first]->pass->GetResourceDescription(lastUsed.second).imageLayout;

        if (r->resourceRef.IsType(ResourceType::Image))
        {
            GPUBarrier initialLayoutTransfer{
                .image = (Gfx::Image*)r->resourceRef.GetResource(),
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
            RenderPass::ResourceHandle handle = p.second;
            auto& desc = sortedNodes[sortIndex]->pass->GetResourceDescription(handle);
            if (r->resourceRef.IsType(ResourceType::Image))
            {
                Gfx::Image* image = (Gfx::Image*)r->resourceRef.GetResource();

                Gfx::PipelineStageFlags srcStages = Gfx::PipelineStage::Top_Of_Pipe;
                Gfx::AccessMask srcAccessMask = Gfx::AccessMask::None;

                if (Gfx::HasWriteAccessMask(desc.accessFlags) || currentLayout != desc.imageLayout)
                {
                    // write after write
                    for (UsedIndex i : writeAccess)
                    {
                        SortIndex srcWriteSortIndex = r->used[i].first;
                        auto& srcWriteDesc = sortedNodes[srcWriteSortIndex]->pass->GetResourceDescription(handle);
                        srcAccessMask |= srcWriteDesc.accessFlags;
                        srcStages |= srcWriteDesc.stageFlags;
                    }

                    // write after read
                    for (UsedIndex i : readAccess)
                    {
                        SortIndex srcReadSortIndex = r->used[i].first;
                        auto& srcReadDesc = sortedNodes[srcReadSortIndex]->pass->GetResourceDescription(handle);
                        srcAccessMask |= srcReadDesc.accessFlags;
                        srcStages |= srcReadDesc.stageFlags;
                    }

                    writeAccess.push_back(usedIndex);
                }

                if (Gfx::HasReadAccessMask(desc.accessFlags))
                {
                    // read after write
                    for (UsedIndex i : writeAccess)
                    {
                        SortIndex srcWriteSortIndex = r->used[i].first;
                        auto& srcWriteDesc = sortedNodes[srcWriteSortIndex]->pass->GetResourceDescription(handle);
                        srcAccessMask |= srcWriteDesc.accessFlags;
                        srcStages |= srcWriteDesc.stageFlags;
                    }

                    readAccess.push_back(usedIndex);
                }

                if (srcStages != Gfx::PipelineStage::None || srcAccessMask != Gfx::AccessMask::None ||
                    currentLayout != desc.imageLayout)
                {
                    GPUBarrier barrier{
                        .image = (Gfx::Image*)r->resourceRef.GetResource(),
                        .srcStageMask = srcStages,
                        .dstStageMask = desc.stageFlags,
                        .srcAccessMask = srcAccessMask,
                        .dstAccessMask = desc.accessFlags,
                        .imageInfo = {
                            .srcQueueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED,
                            .dstQueueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED,
                            .oldLayout = currentLayout,
                            .newLayout = desc.imageLayout,
                            .subresourceRange = image->GetSubresourceRange()}};

                    barriers[sortIndex].push_back(barrier);
                }

                currentLayout = desc.imageLayout;
            }
        }
    }

    // convert map barriers to vector barriers so that we can safely insert into sortedNodes
    std::vector<std::pair<SortIndex, std::vector<GPUBarrier>>> vecBarriers;
    for (auto& b : barriers)
    {
        vecBarriers.emplace_back(b.first, b.second);
    }

    for (auto& b : vecBarriers)
    {
        SortIndex sortIndex = b.first;
        std::vector<GPUBarrier>& gpuBarriers = b.second;

        std::unique_ptr<RenderPass> pass(new RenderPass(
            [gpuBarriers](CommandBuffer& cmd, auto& b, auto& c)
            // cast to remove const
            { cmd.Barrier((GPUBarrier*)gpuBarriers.data(), gpuBarriers.size()); },
            {},
            {},
            {},
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

    submitFence = GetGfxDriver()->CreateFence({.signaled = true});
    submitSemaphore = GetGfxDriver()->CreateSemaphore({.signaled = false});
    swapchainAcquireSemaphore = GetGfxDriver()->CreateSemaphore({.signaled = false});
    queue = GetGfxDriver()->GetQueue(QueueType::Main).Get();
    commandPool = GetGfxDriver()->CreateCommandPool({.queueFamilyIndex = queue->GetFamilyIndex()});
    mainCmd = commandPool->AllocateCommandBuffers(CommandBufferType::Primary, 1)[0];

    mainCmd->Begin();
    mainCmd->Barrier(initialLayoutTransfers.data(), initialLayoutTransfers.size());
    mainCmd->End();

    RefPtr<CommandBuffer> cmdBufs[] = {mainCmd.get()};
    GetGfxDriver()->QueueSubmit(queue, cmdBufs, {}, {}, {}, nullptr);
    GetGfxDriver()->WaitForIdle();
    commandPool->ResetCommandPool();
}

void Graph::ResourceOwner::Finalize(ResourcePool& pool)
{
    if (request.type == ResourceType::Image)
    {
        if (request.externalImage == nullptr)
        {
            Gfx::Image* image = pool.CreateImage(request.imageCreateInfo, request.imageUsagesFlags);
            resourceRef.SetResource(image);
        }
        else
        {
            resourceRef.SetResource(request.externalImage);
        }
    }
    else if (request.type == ResourceType::Buffer)
    {
        throw std::runtime_error("Not Implemented");
    }
}

Graph::ResourceOwner* Graph::CreateResourceOwner(const RenderPass::ResourceDescription& request)
{
    if (request.type == ResourceType::Image)
    {
        auto owner = std::make_unique<ResourceOwner>(request);
        resourceOwners.push_back(std::move(owner));
        return resourceOwners.back().get();
    }
    else if (request.type == ResourceType::Buffer)
    {
        assert(false && "Not implemented");

        // Gfx::Buffer* buffer = resourcePool.CreateBuffer();
        // auto owner = std::make_unique<ResourceOwner>(buffer);
        // resourceOwners.push_back(std::move(owner));
        return resourceOwners.back().get();
    }

    return nullptr;
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
            for (Port& port : node->GetInputPorts())
            {
                Port* connected = port.GetConnected();
                if (connected)
                {
                    depth += GetDepthOfNode(connected->GetParent());
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
    std::vector<RenderPass::ResourceHandle> resourceHandles;
    resourceHandles.assign(creationRequests.begin(), creationRequests.end());
    resourceHandles.insert(resourceHandles.end(), externalResources.begin(), externalResources.end());

    for (auto& handle : resourceHandles)
    {
        auto& request = n->pass->GetResourceDescription(handle);
        ResourceOwner* owner = CreateResourceOwner(request);
        n->pass->SetResourceRef(request.handle, &owner->resourceRef);
        owner->used.push_back({n->sortIndex, handle});
    }

    // set resource from connected
    for (auto& inPort : n->GetInputPorts())
    {
        Port* connected = inPort.GetConnected();
        if (connected == nullptr)
            throw Errrors::GraphCompile("A node has unconnected port");

        auto resRef = connected->parent->pass->GetResourceRef(connected->handle);
        ResourceOwner* owner = (ResourceOwner*)resRef->GetOwner();
        owner->used.push_back({inPort.parent->sortIndex, inPort.handle});

        auto& fromDesc = n->pass->GetResourceDescription(connected->handle);
        owner->request.imageUsagesFlags |= fromDesc.imageUsagesFlags;
        owner->request.bufferUsageFlags |= fromDesc.bufferUsageFlags;

        inPort.parent->pass->SetResourceRef(inPort.handle, resRef);
    }
}

void Graph::Compile() {}

Gfx::Buffer* Graph::ResourcePool::CreateBuffer()
{
    return nullptr;
}
Gfx::Image* Graph::ResourcePool::CreateImage(const Gfx::ImageDescription& imageDesc, Gfx::ImageUsageFlags usages)
{
    images.push_back(std::move(GetGfxDriver()->CreateImage(imageDesc, usages)));

    return images.back().get();
}

void Graph::ResourcePool::ReleaseBuffer(Gfx::Buffer* handle) {}

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

void Graph::Execute()
{
    GetGfxDriver()->WaitForFence({submitFence}, true, -1);
    submitFence->Reset();
    commandPool->ResetCommandPool();

    GetGfxDriver()->AcquireNextSwapChainImage(swapchainAcquireSemaphore);
    mainCmd->Begin();
    // TODO: better move the whole gfx thing to a renderer
    mainCmd->SetViewport({.x = 0, .y = 0, .width = 1920, .height = 1080, .minDepth = 0, .maxDepth = 1});
    Rect2D rect = {{0, 0}, {1920, 1080}};
    mainCmd->SetScissor(0, 1, &rect);
    for (auto n : sortedNodes)
    {
        n->pass->Execute(*mainCmd);
    }
    mainCmd->End();

    RefPtr<CommandBuffer> cmds[] = {mainCmd};
    RefPtr<Gfx::Semaphore> waitSemaphores[] = {swapchainAcquireSemaphore};
    Gfx::PipelineStageFlags waitPipelineStages[] = {Gfx::PipelineStage::Color_Attachment_Output};
    RefPtr<Gfx::Semaphore> submitSemaphores[] = {submitSemaphore.get()};
    GetGfxDriver()->QueueSubmit(queue, cmds, waitSemaphores, waitPipelineStages, submitSemaphores, submitFence);

    GetGfxDriver()->Present({submitSemaphore});
}
} // namespace Engine::RenderGraph
