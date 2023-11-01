#include "FrameGraph.hpp"

namespace Engine::FrameGraph
{
void Graph::BuildGraph(Graph& graph, nlohmann::json& j)
{
    nlohmann::json nodes = j.value("nodes", {});
    if (!nodes.is_null())
    {
        for (auto& n : nodes)
        {
            std::string id = j.value("nodeID", "");
        }
    }
}

Node& Graph::AddNode(const NodeBlueprint& bp)
{
    auto n = bp.CreateNode(nodeIDPool.Allocate() << FRAME_GRAPH_PROPERTY_BIT_COUNT);
    auto t = n.get();
    nodes.push_back(std::move(n));
    return *t;
}

bool Graph::Connect(FGID src, FGID dst)
{
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
    FGID nodeID = node->GetID();
    nodeIDPool.Release(node->GetID());
    nodes.erase(std::find_if(nodes.begin(), nodes.end(), [node](std::unique_ptr<Node>& n) { return n.get() == node; }));

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
    auto iter = std::find(connections.begin(), connections.end(), connectionID);
    connections.erase(iter);
}

std::span<FGID> Graph::GetConnections()
{
    return connections;
}

} // namespace Engine::FrameGraph
