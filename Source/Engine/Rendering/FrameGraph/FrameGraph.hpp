#pragma once
#include "NodeBlueprint.hpp"
#include "Nodes/Node.hpp"
#include "Rendering/RenderGraph/Graph.hpp"
#include <nlohmann/json.hpp>
#include <span>
#include <vector>

namespace Engine::FrameGraph
{

class Graph
{
public:
    Graph(){};
    void BuildGraph(Graph& graph, nlohmann::json& j);
    void Execute(Graph& graph);
    bool Connect(FGID src, FGID dst);
    Node& AddNode(const NodeBlueprint& bp);
    void DeleteNode(Node* node);
    void DeleteNode(FGID id);
    void DeleteConnection(FGID connectionID);

    std::span<FGID> GetConnections();

    std::span<std::unique_ptr<Node>> GetNodes()
    {
        return nodes;
    }

    static FGID GetSrcNodeIDFromConnect(FGID connectionID)
    {
        return (connectionID >> FRAME_GRAPH_NODE_PROPERTY_BIT_COUNT) & FRAME_GRAPH_NODE_BIT_MASK;
    }

    static FGID GetDstNodeIDFromConnect(FGID connectionID)
    {
        return connectionID & FRAME_GRAPH_NODE_BIT_MASK;
    }

    static FGID GetSrcPropertyIDFromConnectionID(FGID connectionID)
    {
        return connectionID >> FRAME_GRAPH_NODE_PROPERTY_BIT_COUNT;
    }

    static FGID GetDstPropertyIDFromConnectionID(FGID connectionID)
    {
        return connectionID & FRAME_GRAPH_PROPERTY_BIT_MASK;
    }

    static FGID GetNodeID(FGID id)
    {
        return id & FRAME_GRAPH_NODE_BIT_MASK;
    }

    static FGID GetPropertyID(FGID id)
    {
        return id & FRAME_GRAPH_PROPERTY_BIT_MASK;
    }

private:
    using RGraph = RenderGraph::Graph;

    RGraph* targetGraph;
    std::vector<FGID> connections;
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

        uint16_t Allocate()
        {
            if (freeID.empty())
            {
                throw std::logic_error("Maximum id reached");
            }

            uint16_t back = freeID.back();
            freeID.pop_back();
            return back;
        };

        void Release(FGID v)
        {
            freeID.push_back(v);
        }

        const std::vector<uint16_t> GetFreeIDs()
        {
            return freeID;
        }

    private:
        std::vector<uint16_t> freeID;
    };

    IDPool nodeIDPool;

    bool HasCycleIfLink(FGID src, FGID dst)
    {
        return false; // not implemented
    }
};

} // namespace Engine::FrameGraph
