#include "Graph.hpp"
#include "Errors.hpp"

#include <algorithm>

namespace Engine::RenderGraph
{
RenderNode::RenderNode(std::unique_ptr<RenderPass>&& pass_) : pass(std::move(pass_))
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

void Graph::Process()
{
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

    // copy
    this->sortedNodes = sortedNodes;

    submitFence = GetGfxDriver()->CreateFence({.signaled = true});
    submitSemaphore = GetGfxDriver()->CreateSemaphore({.signaled = false});
    swapchainAcquireSemaphore = GetGfxDriver()->CreateSemaphore({.signaled = false});
    queue = GetGfxDriver()->GetQueue(QueueType::Main).Get();
    commandPool = GetGfxDriver()->CreateCommandPool({.queueFamilyIndex = queue->GetFamilyIndex()});
    mainCmd = commandPool->AllocateCommandBuffers(CommandBufferType::Primary, 1)[0];
}

Graph::ResourceOwner* Graph::CreateResourceOwner(const RenderPass::ResourceDescription& request, int originalNode)
{
    if (request.type == ResourceType::Image)
    {
        Gfx::Image* image = resourcePool.CreateImage(request.imageCreateInfo, request.imageUsagesFlags);
        auto owner = std::make_unique<ResourceOwner>(image);
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
    // create resource
    auto creationRequests = n->pass->GetResourceCreationRequests();
    for (auto& handle : creationRequests)
    {
        auto& request = n->pass->GetResourceDescription(handle);
        ResourceOwner* owner = CreateResourceOwner(request, n->sortIndex);
        n->pass->SetResourceRef(request.handle, owner->resourceRef);
        owner->used[n->sortIndex] = true;
    }

    // set resource from connected
    for (auto& inPort : n->GetInputPorts())
    {
        Port* connected = inPort.GetConnected();
        if (connected == nullptr)
            throw Errrors::GraphCompile("A node has unconnected port");

        auto resRef = connected->parent->pass->GetResourceRef(connected->handle);
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
    GetGfxDriver()->AcquireNextSwapChainImage(swapchainAcquireSemaphore);
    for (auto n : sortedNodes)
    {
        n->pass->Execute(*mainCmd);
    }

    RefPtr<CommandBuffer> cmds[] = {mainCmd};
    RefPtr<Gfx::Semaphore> waitSemaphores[] = {swapchainAcquireSemaphore};
    Gfx::PipelineStageFlags waitPipelineStages[] = {Gfx::PipelineStage::Color_Attachment_Output};
    RefPtr<Gfx::Semaphore> submitSemaphores[] = {submitSemaphore.get()};
    GetGfxDriver()->QueueSubmit(queue, cmds, waitSemaphores, waitPipelineStages, submitSemaphores, submitFence);

    GetGfxDriver()->Present({submitSemaphore});
}
} // namespace Engine::RenderGraph
