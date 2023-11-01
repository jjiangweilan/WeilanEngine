#pragma once
#include "Rendering/FrameGraph/FrameGraph.hpp"
#include "ThirdParty/imgui/imguinode/imgui_node_editor.h"
#include <filesystem>
#include <nlohmann/json.hpp>

namespace Engine::Editor
{

struct FrameGraphNode
{};

class FrameGraphEditor
{
public:
    void Draw();
    void Init();
    void Destory();

    void SetGraph(FrameGraph::Graph& graph)
    {
        this->graph = &graph;
    }

private:
    std::unique_ptr<FrameGraph::Graph> testGraph;
    FrameGraph::Graph* graph = nullptr;
    ax::NodeEditor::EditorContext* graphContext;
    struct LinkInfo
    {
        ax::NodeEditor::LinkId Id;
        ax::NodeEditor::PinId InputId;
        ax::NodeEditor::PinId OutputId;
    };

    void DrawProperty(FrameGraph::Property& p, ax::NodeEditor::PinKind kind);
    void DrawFloatProp(FrameGraph::Property& p);
    void DrawImageProp(FrameGraph::Property& p);

    void DrawFloatConfig();
    void DrawVectorConfig();
};
} // namespace Engine::Editor
