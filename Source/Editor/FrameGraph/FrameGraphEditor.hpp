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
    FrameGraph::Graph* graph = nullptr;
    ax::NodeEditor::EditorContext* graphContext;
};
} // namespace Engine::Editor
