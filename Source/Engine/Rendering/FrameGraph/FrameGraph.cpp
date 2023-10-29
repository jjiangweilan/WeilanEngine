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
    auto n = bp.CreateNode(nodeIDPool.Allocate());
    auto t = n.get();
    nodes.push_back(std::move(n));
    return *t;
}

bool Graph::Connect(PinID src, PinID dst)
{
    return true;
}

void Graph::DeleteNode(Node* node)
{
    nodeIDPool.Release(node->GetID());
    nodes.erase(std::find_if(nodes.begin(), nodes.end(), [node](std::unique_ptr<Node>& n) { return n.get() == node; }));
}

std::span<Connection> Graph::GetConnections()
{
    return connections;
}

} // namespace Engine::FrameGraph
