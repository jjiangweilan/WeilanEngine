#pragma once
#include "Rendering/FrameGraph/FrameGraph.hpp"
#include "ThirdParty/imgui/imguinode/imgui_node_editor.h"
#include <filesystem>
#include <nlohmann/json.hpp>

namespace Editor
{

struct FrameGraphNode
{};

class FrameGraphEditor
{
public:
    void ShowOutputImages();
    void Draw(ax::NodeEditor::EditorContext* context, Rendering::FrameGraph::Graph& graph);

private:
    Rendering::FrameGraph::Graph* graph = nullptr;
    ax::NodeEditor::NodeId nodeContext;

    struct LinkInfo
    {
        ax::NodeEditor::LinkId Id;
        ax::NodeEditor::PinId InputId;
        ax::NodeEditor::PinId OutputId;
    };

    void DrawProperty(Rendering::FrameGraph::Property& p, ax::NodeEditor::PinKind kind);
    void DrawFloatProp(Rendering::FrameGraph::Property& p);
    void DrawImageProp(Rendering::FrameGraph::Property& p);

    void DrawFloatConfig();
    void DrawVectorConfig();

    void DrawConfigurableField(
        bool& openImageFormatPopup,
        const Rendering::FrameGraph::Configurable*& targetConfig,
        const Rendering::FrameGraph::Configurable& config
    );
};
} // namespace Editor
