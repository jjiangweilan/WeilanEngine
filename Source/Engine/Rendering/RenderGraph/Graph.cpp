#include "Graph.hpp"
#include "GfxDriver/GfxDriver.hpp"

namespace Engine::RenderGraph
{

Graph::Graph() { context = std::make_unique<GraphContext>(); }

Graph::~Graph() {}

bool Graph::Connect(Port* p0, Port* p1)
{
    if (p0 == p1 || p0->parent == p1->parent || p0->type != p1->type)
        return false;

    Port* src = p0->isOutput ? p0 : p1;
    Port* dst = p0->isOutput ? p1 : p0;

    src->connected = dst;
    dst->connected = src;

    SetInputNodeResource(src, dst);

    return true;
}

void Graph::SetInputNodeResource(Port* src, Port* dst)
{
    dst->resourceRef = src->resourceRef;

    // record output ports' resource status
    auto dstNodeOutputPorts = dst->GetParent()->GetOutputPorts();
    std::vector<ResourceRef> res(dstNodeOutputPorts.size());
    for (int i = 0; i < dstNodeOutputPorts.size(); ++i)
    {
        res[i] = dstNodeOutputPorts[i]->resourceRef;
    }

    if (dst->onResourceChange)
        dst->onResourceChange(dst);

    for (int i = 0; i < dstNodeOutputPorts.size(); ++i)
    {
        if (res[i] != dstNodeOutputPorts[i]->resourceRef)
        {
            UpdateConnectedNode(dstNodeOutputPorts[i]);
        }
    }
}

void Graph::UpdateConnectedNode(Port* output)
{
    if (output->connected)
    {
        SetInputNodeResource(output, output->connected);
    }
}

static int GetNodeDepth(Node* node, std::unordered_map<Node*, int>& depth)
{
    auto depthIter = depth.find(node);
    if (depthIter != depth.end())
    {
        return depthIter->second;
    }

    int depthNum = 1;
    for (auto input : node->GetInputPorts())
    {
        if (input->GetConnected())
        {
            depthNum += GetNodeDepth(input->GetConnected()->GetParent(), depth);
        }
    }

    depth[node] = depthNum;
    return depthNum;
}

std::unordered_map<Node*, int> Graph::CalculateNodeDepth(std::vector<std::unique_ptr<Node>>& nodes)
{
    std::unordered_map<Node*, int> depth;

    for (auto& n : nodes)
    {
        GetNodeDepth(n.get(), depth);
    }

    return depth;
}

void Graph::Preprocess() {}
} // namespace Engine::RenderGraph
