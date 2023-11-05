#pragma once
#include "Core/Asset.hpp"
#include "NodeBlueprint.hpp"
#include "Nodes/Node.hpp"
#include "Rendering/RenderGraph/Graph.hpp"
#if ENGINE_EDITOR
#include "ThirdParty/imgui/imguinode/imgui_node_editor.h"
#endif
#include <nlohmann/json.hpp>
#include <span>
#include <vector>

namespace Engine::FrameGraph
{

class Graph : public Asset
{
    DECLARE_ASSET();

public:
    Graph()
    {
#if ENGINE_EDITOR
        ax::NodeEditor::Config config;
        config.SettingsFile = "Frame Graph Editor.json";
        graphContext = ax::NodeEditor::CreateEditor(&config);
#endif
    };
    ~Graph()
    {
#if ENGINE_EDITOR
        ax::NodeEditor::DestroyEditor(graphContext);
#endif
    }

#if ENGINE_EDITOR
    ax::NodeEditor::EditorContext* GetEditorContext()
    {
        return graphContext;
    }
#endif
    void Execute(Graph& graph);
    bool Connect(FGID src, FGID dst);
    Node& AddNode(const NodeBlueprint& bp);
    void DeleteNode(Node* node);
    void DeleteNode(FGID id);
    void DeleteConnection(FGID connectionID);
    std::span<FGID> GetConnections();
    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;

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
    class IDPool : public Serializable
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

        void Release(FGID v)
        {
            freeID.push_back(v);
        }

        const std::vector<uint32_t> GetFreeIDs()
        {
            return freeID;
        }

        void Serialize(Serializer* s) const override
        {
            s->Serialize("freeID", freeID);
        }

        void Deserialize(Serializer* s) override
        {
            s->Deserialize("freeID", freeID);
        }

    private:
        std::vector<uint32_t> freeID;
    };

    IDPool nodeIDPool;
    std::vector<FGID> connections;
    std::vector<std::unique_ptr<Node>> nodes;
#if ENGINE_EDITOR
    ax::NodeEditor::EditorContext* graphContext;
#endif

    bool HasCycleIfLink(FGID src, FGID dst)
    {
        return false; // not implemented
    }
};

} // namespace Engine::FrameGraph
