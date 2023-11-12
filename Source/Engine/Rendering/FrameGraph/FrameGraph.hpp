#pragma once
#include "Asset/Shader.hpp"
#include "Core/Asset.hpp"
#include "Core/Scene/Scene.hpp"
#include "GraphResource.hpp"
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
        SetName("New Frame Graph");
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

    void Execute(Gfx::CommandBuffer& cmd, Scene& scene);
    bool Connect(FGID src, FGID dst);
    Node& AddNode(const NodeBlueprint& bp);
    void DeleteNode(Node* node);
    void DeleteNode(FGID id);
    void DeleteConnection(FGID connectionID);
    std::span<FGID> GetConnections();
    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    bool Compile();
    void SetOutputImageNode(FGID nodeID);
    Node* GetNode(FGID nodeID);
    Gfx::Image* GetOutputImage();
    Node* GetOutputImageNode()
    {
        return outputImageNode;
    }

    Shader* GetTemplateSceneShader()
    {
        return templateSceneResourceShader;
    };

    void SetTemplateSceneShader(Shader* shader)
    {
        SetDirty();
        this->templateSceneResourceShader = shader;
    }

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

    void ReportValidation();

    bool IsCompiled()
    {
        return compiled;
    }

private:
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

    struct LightInfo
    {
        glm::vec4 position;
        float range;
        float intensity;
    };

    static const int MAX_LIGHT_COUNT = 32; // defined in Commom.glsl
    struct SceneInfo
    {
        glm::vec4 viewPos;
        glm::mat4 view;
        glm::mat4 projection;
        glm::mat4 viewProjection;
        glm::mat4 worldToShadow;
        glm::vec4 lightCount;
        glm::vec4 shadowMapSize;
        LightInfo lights[MAX_LIGHT_COUNT];
    } sceneInfo;

    IDPool nodeIDPool;
    std::vector<FGID> connections;
    std::vector<std::unique_ptr<Node>> nodes;
#if ENGINE_EDITOR
    ax::NodeEditor::EditorContext* graphContext;
#endif
    bool compiled = false;
    Node* outputImageNode = nullptr;
    GraphResource graphResource;
    Shader* templateSceneResourceShader;

    std::unique_ptr<RenderGraph::Graph> graph;
    std::unique_ptr<Gfx::ShaderResource> sceneShaderResource{};
    Gfx::Buffer* sceneGlobalBuffer;
    std::unique_ptr<Gfx::Buffer> stagingBuffer;

    bool HasCycleIfLink(FGID src, FGID dst)
    {
        return false; // not implemented
    }

    void ProcessLights(Scene& gameScene);
    void RequireRecompile()
    {
        compiled = false;
    }
};

} // namespace Engine::FrameGraph
