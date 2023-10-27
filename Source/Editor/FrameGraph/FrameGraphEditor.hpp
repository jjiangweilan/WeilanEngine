#pragma once
#include "Rendering/FrameGraph/FrameGraph.hpp"
#include <filesystem>
#include <nlohmann/json.hpp>

namespace Engine::Editor
{

struct FrameGraphNode
{};

class FrameGraphEditor
{
public:
    FrameGraphEditor();

    void Draw();

    void SetGraph(FrameGraph::Graph& graph)
    {
        this->graph = &graph;
    }

private:
    FrameGraph::Graph* graph = nullptr;
};
} // namespace Engine::Editor
