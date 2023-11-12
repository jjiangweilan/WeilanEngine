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
    void Draw(ax::NodeEditor::EditorContext* context, FrameGraph::Graph& graph);

private:
    FrameGraph::Graph* graph = nullptr;
    ax::NodeEditor::NodeId nodeContext;

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

    void DrawConfigurableField(
        bool& openImageFormatPopup,
        const FrameGraph::Configurable*& targetConfig,
        const FrameGraph::Configurable& config
    );
};
} // namespace Engine::Editor
