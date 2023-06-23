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
    auto& srcDesc = srcNode->pass->GetReferenceResourceDescription(src.handle);
    auto& dstDesc = dstNode->pass->GetReferenceResourceDescription(dst.handle);

    if (srcDesc.type != dstDesc.type)
        return false;

    src.connected = &dst;
    dst.connected = &src;

    return true;
}

void Graph::Process()
{
    std::vector<RenderNode*> nodesSorted(nodes.size());
    std::transform(nodes.begin(), nodes.end(), nodesSorted.begin(), [](auto& n) { return n.get(); });

    // recalculate depth
    for (RenderNode* n : nodesSorted)
        n->depth = -1;
    for (RenderNode* n : nodesSorted)
        n->depth = GetDepthOfNode(n);

    // sort nodes
    std::sort(nodesSorted.begin(), nodesSorted.end(), [](RenderNode* l, RenderNode* r) { return l->depth < r->depth; });
    for (int i = 0; i < nodesSorted.size(); ++i)
        nodesSorted[i]->sortIndex = i;

    // preprocess
    for (RenderNode* n : nodesSorted)
    {
        Preprocess(n);
    }
}

Graph::ResourceOwner* Graph::CreateResourceOwner(const RenderPass::ResourceCreationRequest& request, int originalNode)
{
    if (request.type == ResourceType::Image)
    {
        Gfx::Image* image = resourcePool.CreateImage();
        auto owner = std::make_unique<ResourceOwner>(image);
        resourceOwners.push_back(std::move(owner));
        return resourceOwners.back().get();
    }
    else if (request.type == ResourceType::Buffer)
    {
        Gfx::Buffer* buffer = resourcePool.CreateBuffer();
        auto owner = std::make_unique<ResourceOwner>(buffer);
        resourceOwners.push_back(std::move(owner));
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
    for (auto& request : creationRequests)
    {
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
} // namespace Engine::RenderGraph
