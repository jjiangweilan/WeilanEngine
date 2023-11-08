#include "FrameGraph.hpp"
#include <spdlog/spdlog.h>

namespace Engine::FrameGraph
{
DEFINE_ASSET(Graph, "C18AC918-98D0-41BF-920D-DE0FD7C06029", "fgraph");

Node& Graph::AddNode(const NodeBlueprint& bp)
{
    SetDirty();
    auto n = bp.CreateNode(nodeIDPool.Allocate() << FRAME_GRAPH_PROPERTY_BIT_COUNT);
    auto t = n.get();
    nodes.push_back(std::move(n));
    return *t;
}

bool Graph::Connect(FGID src, FGID dst)
{
    SetDirty();

    if (src == dst)
        return false;

    FGID srcNodeID = GetNodeID(src);
    FGID dstNodeID = GetNodeID(dst);

    auto srcIter = std::find_if(
        nodes.begin(),
        nodes.end(),
        [srcNodeID](std::unique_ptr<Node>& n) { return n->GetID() == srcNodeID; }
    );
    auto dstIter = std::find_if(
        nodes.begin(),
        nodes.end(),
        [dstNodeID](std::unique_ptr<Node>& n) { return n->GetID() == dstNodeID; }
    );
    if (srcIter == nodes.end() || dstIter == nodes.end() || srcIter == dstIter)
        return false;

    std::unique_ptr<Node>& srcNode = *srcIter;
    std::unique_ptr<Node>& dstNode = *dstIter;

    Property* srcProp = srcNode->GetProperty(src);
    Property* dstProp = dstNode->GetProperty(dst);

    if (srcProp == nullptr || dstProp == nullptr)
        return false;
    if ((srcProp->GetType() != dstProp->GetType()) || (srcProp->IsOuput() && dstProp->IsOuput()) ||
        (srcProp->IsInput() && dstProp->IsInput()))
        return false;

    if (HasCycleIfLink(src, dst))
    {
        return false;
    }

    connections.push_back(src << FRAME_GRAPH_NODE_PROPERTY_BIT_COUNT | dst);

    return true;
}

void Graph::DeleteNode(Node* node)
{
    SetDirty();

    auto nodeIter =
        std::find_if(nodes.begin(), nodes.end(), [node](std::unique_ptr<Node>& n) { return n.get() == node; });
    if (nodeIter == nodes.end())
    {
        throw std::logic_error("Deleted a non-existing node");
    }
    FGID nodeID = node->GetID();
    nodes.erase(nodeIter);
    nodeIDPool.Release(nodeID >> FRAME_GRAPH_PROPERTY_BIT_COUNT);

    auto iter = std::remove_if(
        connections.begin(),
        connections.end(),

        [nodeID](FGID& c)
        {
            FGID srcNode = GetSrcNodeIDFromConnect(c);
            FGID dstNode = GetDstNodeIDFromConnect(c);
            return srcNode == nodeID || dstNode == nodeID;
        }
    );

    connections.erase(iter, connections.end());
}

void Graph::DeleteConnection(FGID connectionID)
{
    SetDirty();

    auto iter = std::find(connections.begin(), connections.end(), connectionID);
    connections.erase(iter);
}

std::span<FGID> Graph::GetConnections()
{
    return connections;
}

void Graph::DeleteNode(FGID id)
{
    auto iter = std::find_if(nodes.begin(), nodes.end(), [id](std::unique_ptr<Node>& n) { return n->GetID() == id; });
    DeleteNode(iter->get());
}

void Graph::Serialize(Serializer* s) const
{
    Asset::Serialize(s);
    s->Serialize("nodeIDPool", nodeIDPool);
    s->Serialize("connections", connections);
    s->Serialize("nodes", nodes);
}

void Graph::Deserialize(Serializer* s)
{
    Asset::Deserialize(s);
    s->Deserialize("nodeIDPool", nodeIDPool);
    s->Deserialize("connections", connections);
    s->Deserialize("nodes", nodes);
}

void Graph::ReportValidation()
{
    std::unordered_map<FGID, int> counts;
    for (auto c : connections)
    {
        counts[c] += 1;
    }

    for (auto& n : nodes)
    {
        counts[n->GetID()] += 1;
        for (auto& p : n->GetInput())
        {
            counts[p.GetID()] += 1;
        }

        for (auto& p : n->GetOutput())
        {
            counts[p.GetID()] += 1;
        }
    }

    for (auto& iter : counts)
    {
        spdlog::info("{}, {}", iter.first, iter.second);
    }
}

void Graph::Execute(Gfx::CommandBuffer& cmd)
{
    for (auto& n : nodes)
    {
        n->Execute(graphResource);
    }

    graph->Execute(cmd);
}

void Graph::Compile()
{
    GetGfxDriver()->WaitForIdle();
    graph = std::make_unique<RenderGraph::Graph>();

    Resources buildResources;

    for (auto& n : nodes)
    {
        auto resources = n->Preprocess(*graph);
        for (auto& r : resources)
        {
            if (r.type == ResourceType::Forwarding)
            {
                throw std::runtime_error("Not implemented");
            }
            else
            {
                for (auto c : connections)
                {
                    if (GetSrcPropertyIDFromConnectionID(c) == r.propertyID)
                    {
                        buildResources.resources.emplace(GetDstPropertyIDFromConnectionID(c), r);
                    }
                }
            }
        }
    }

    for (auto& n : nodes)
    {
        n->Build(*graph, buildResources);
    }

    graph->Process();

    for (auto& n : nodes)
    {
        n->Finalize(*graph, buildResources);
    }
}
} // namespace Engine::FrameGraph
