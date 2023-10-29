#include "FrameGraphEditor.hpp"
#include "ThirdParty/imgui/GraphEditor.h"

namespace Engine::Editor
{
void FrameGraphEditor::Init()
{
    ax::NodeEditor::Config config;
    config.SettingsFile = "Simple.json";
    graphContext = ax::NodeEditor::CreateEditor(&config);
}

void FrameGraphEditor::Destory()
{
    ax::NodeEditor::DestroyEditor(graphContext);
}

void FrameGraphEditor::Draw()
{
    ax::NodeEditor::SetCurrentEditor(graphContext);
    ax::NodeEditor::Begin("Frame Graph");

    int uniqueId = 1;
    if (graph)
    {
        for (auto& node : graph->GetNodes())
        {}
    }

    ax::NodeEditor::BeginNode(uniqueId++);
    ImGui::Text("Node A");
    ax::NodeEditor::BeginPin(uniqueId++, ax::NodeEditor::PinKind::Input);
    ImGui::Text("-> In");
    ax::NodeEditor::EndPin();
    ImGui::SameLine();
    ax::NodeEditor::BeginPin(uniqueId++, ax::NodeEditor::PinKind::Output);
    ImGui::Text("Out ->");
    ax::NodeEditor::EndPin();
    ax::NodeEditor::EndNode();
    ax::NodeEditor::End();
    ax::NodeEditor::SetCurrentEditor(nullptr);
}
} // namespace Engine::Editor
