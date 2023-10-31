#pragma once
#include "NodeBlueprint.hpp"
#include "Nodes/Node.hpp"
#include "Rendering/RenderGraph/Graph.hpp"
#include <nlohmann/json.hpp>
#include <span>
#include <vector>

namespace Engine::FrameGraph
{
using PinID = int;
using ConnectionID = int;

struct Connection
{
    ConnectionID id;
    PinID src;
    PinID dst;
};

class Graph
{
public:
    Graph(){};
    void BuildGraph(Graph& graph, nlohmann::json& j);
    void Execute(Graph& graph);
    bool Connect(PinID src, PinID dst);
    Node& AddNode(const NodeBlueprint& bp);
    void DeleteNode(Node* node);

    std::span<Connection> GetConnections();

    std::span<std::unique_ptr<Node>> GetNodes()
    {
        return nodes;
    }

private:
    using RGraph = RenderGraph::Graph;

    RGraph* targetGraph;
    std::vector<Connection> connections;
    std::vector<std::unique_ptr<Node>> nodes;

    class IDPool
    {
    public:
        IDPool(int capacity)
        {
            freeID.resize(capacity);
            for (int i = 0; i < capacity; ++i)
                freeID[i] = i;
        }

        IDPool() : IDPool(256) {}

        uint32_t Allocate()
        {
            if (freeID.empty())
            {
                throw std::logic_error("Maximum id reached");
            }

            uint32_t back = freeID.back();
            freeID.pop_back();
            return back;
        };

        void Release(uint32_t v)
        {
            freeID.push_back(v);
        }

        const std::vector<uint32_t> GetFreeIDs()
        {
            return freeID;
        }

    private:
        std::vector<uint32_t> freeID;
    };

    IDPool nodeIDPool;
};

} // namespace Engine::FrameGraph
