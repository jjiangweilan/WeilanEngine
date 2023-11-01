#include "FrameGraphEditor.hpp"
#include "ThirdParty/imgui/GraphEditor.h"
#include <spdlog/spdlog.h>
namespace Engine::Editor
{
namespace ed = ax::NodeEditor;
namespace fg = FrameGraph;

void FrameGraphEditor::Init()
{
    ax::NodeEditor::Config config;
    config.SettingsFile = "Frame Graph Editor.json";

    graphContext = ax::NodeEditor::CreateEditor(&config);

    testGraph = std::make_unique<FrameGraph::Graph>();
    graph = testGraph.get();
}

void FrameGraphEditor::Destory()
{
    ax::NodeEditor::DestroyEditor(graphContext);
}

void FrameGraphEditor::Draw()
{
    ed::SetCurrentEditor(graphContext);
    auto cursorPosition = ImGui::GetCursorScreenPos();

    ImGui::Begin("Frame Graph Editor");
    ed::Begin("Frame Graph");
    {
        if (graph)
        {
            int uniqueId = 0;
            for (std::unique_ptr<FrameGraph::Node>& node : graph->GetNodes())
            {
                uniqueId += 1;
                ed::BeginNode(node->GetID());

                ImGui::Indent(50);
                ImGui::PushItemWidth(280);
                ImGui::Text("Node Type: %s", node->GetName().c_str());
                char customName[1024];
                strcpy(customName, node->GetCustomName().c_str());
                ImGui::PushID(uniqueId);
                if (ImGui::InputText("name", customName, 1024))
                {
                    node->SetCustomName(customName);
                }
                ImGui::PopID();

                // configurable block
                int pushID = 0;
                for (auto& config : node->GetConfiurables())
                {
                    ImGui::Text("%s", config.name.c_str());
                    ImGui::PushID(pushID);
                    if (config.type == fg::ConfigurableType::Float)
                    {
                        float v = std::any_cast<float>(config.data);
                        if (ImGui::InputFloat("", &v))
                        {
                            config.data = v;
                        }
                    }
                    else if (config.type == fg::ConfigurableType::Vec2)
                    {
                        glm::vec2 v = std::any_cast<glm::vec2>(config.data);
                        if (ImGui::InputFloat2("", &v[0]))
                        {
                            config.data = v;
                        }
                    }
                    else if (config.type == fg::ConfigurableType::Vec3)
                    {
                        glm::vec3 v = std::any_cast<glm::vec3>(config.data);
                        if (ImGui::InputFloat3("", &v[0]))
                        {
                            config.data = v;
                        }
                    }
                    else if (config.type == fg::ConfigurableType::Vec4)
                    {
                        glm::vec4 v = std::any_cast<glm::vec4>(config.data);
                        if (ImGui::InputFloat4("", &v[0]))
                        {
                            config.data = v;
                        }
                    }
                    else if (config.type == fg::ConfigurableType::Vec2Int)
                    {
                        glm::ivec2 v = std::any_cast<glm::ivec2>(config.data);
                        if (ImGui::InputInt2("", &v[0]))
                        {
                            config.data = v;
                        }
                    }
                    else if (config.type == fg::ConfigurableType::Vec3Int)
                    {
                        glm::ivec3 v = std::any_cast<glm::ivec3>(config.data);
                        if (ImGui::InputInt3("", &v[0]))
                        {
                            config.data = v;
                        }
                    }
                    else if (config.type == fg::ConfigurableType::Vec4Int)
                    {
                        glm::ivec4 v = std::any_cast<glm::ivec4>(config.data);
                        if (ImGui::InputInt4("", &v[0]))
                        {
                            config.data = v;
                        }
                    }
                    else if (config.type == fg::ConfigurableType::Format)
                    {
                        Gfx::ImageFormat v = std::any_cast<Gfx::ImageFormat>(config.data);
                        if (ImGui::BeginCombo("", Gfx::MapImageFormatToString(v)))
                        {
                            for (int i = 0; i <= static_cast<int>(Gfx::ImageFormat::Invalid); ++i)
                            {
                                const bool is_selected = (static_cast<int>(v) == i);
                                if (ImGui::Selectable(
                                        Gfx::MapImageFormatToString(static_cast<Gfx::ImageFormat>(i)),
                                        is_selected
                                    ))
                                {
                                    config.data = static_cast<Gfx::ImageFormat>(i);
                                }
                            }
                            ImGui::EndCombo();
                        }
                    }

                    ImGui::PopID();
                    pushID += 1;
                }
                ImGui::PopItemWidth();
                ImGui::Unindent(50);
                for (fg::Property& input : node->GetInput())
                {
                    DrawProperty(input, ed::PinKind::Input);
                }
                ImGui::Spacing();
                for (fg::Property& input : node->GetOutput())
                {
                    DrawProperty(input, ed::PinKind::Output);
                }
                ed::EndNode();
            }
        }

        if (ed::BeginCreate())
        {
            ed::PinId inputPinId, outputPinId;
            if (ed::QueryNewLink(&inputPinId, &outputPinId))
            {
                if (inputPinId && outputPinId)
                {
                    if (ed::AcceptNewItem())
                    {
                        if (!graph->Connect(inputPinId.Get(), outputPinId.Get()))
                        {
                            ed::RejectNewItem();
                        }
                    }
                }
            }
            ed::EndCreate();
        }

        if (ed::ShowBackgroundContextMenu())
        {
            ed::Suspend();
            ImGui::OpenPopup("Create New Node");
            ed::Resume();
        }

        // make links
        for (auto c : graph->GetConnections())
        {
            fg::FGID srcPropID = fg::Graph::GetSrcPropertyIDFromConnectionID(c);
            fg::FGID dstPropID = fg::Graph::GetDstPropertyIDFromConnectionID(c);
            ed::Link(c, srcPropID, dstPropID);
        }
    }

    ImGui::SetCursorScreenPos(cursorPosition);
    // content
    ed::Suspend();
    if (ImGui::BeginPopup("Create New Node"))
    {
        auto popupMousePos = ImGui::GetMousePosOnOpeningCurrentPopup();
        for (fg::NodeBlueprint& bp : fg::NodeBlueprintRegisteration::GetNodeBlueprints())
        {
            if (ImGui::MenuItem(bp.GetName().c_str()))
            {
                FrameGraph::Node& node = graph->AddNode(bp);
                ed::SetNodePosition(node.GetID(), ed::ScreenToCanvas(popupMousePos));
            }
        }
        ImGui::EndPopup();
    }
    ed::Resume();

    ed::End();
    ed::SetCurrentEditor(nullptr);
    ImGui::End();
}

void FrameGraphEditor::DrawFloatProp(FrameGraph::Property& p)
{
    float* v = (float*)p.GetData();
    ImGui::Text("%s", p.GetName().c_str());
    ImGui::PushItemWidth(100);
    if (ImGui::InputFloat(fmt::format("##{}", p.GetID()).c_str(), v))
    {}
    ImGui::PopItemWidth();
}

void FrameGraphEditor::DrawImageProp(FrameGraph::Property& p)
{
    fg::ImageProperty* imageProp = (fg::ImageProperty*)p.GetData();
    ImGui::Text("%s", p.GetName().c_str());
    if (imageProp->WillCreate())
    {
        // shadow create configuration
    }
}

void FrameGraphEditor::DrawProperty(FrameGraph::Property& p, ax::NodeEditor::PinKind kind)
{
    if (kind == ed::PinKind::Output)
        ImGui::Indent(330);
    ed::BeginPin(p.GetID(), kind);
    if (p.GetType() == fg::PropertyType::Float)
        DrawFloatProp(p);
    else if (p.GetType() == fg::PropertyType::Image)
        DrawImageProp(p);
    ed::EndPin();
    if (kind == ed::PinKind::Output)
        ImGui::Unindent(330);
}
} // namespace Engine::Editor
