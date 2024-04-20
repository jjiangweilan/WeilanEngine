#include "FrameGraphEditor.hpp"
#include "EditorState.hpp"
#include "Rendering/FrameGraph/Nodes/ImageNode.hpp"
#include <spdlog/spdlog.h>
namespace Editor
{
namespace ed = ax::NodeEditor;
namespace fg = FrameGraph;

void FrameGraphEditor::DrawConfigurableField(
    bool& openImageFormatPopup, const FrameGraph::Configurable*& targetConfig, const FrameGraph::Configurable& config
)
{
    if (config.type == fg::ConfigurableType::Bool)
    {
        bool v = std::any_cast<bool>(config.data);
        if (ImGui::Checkbox("", &v))
        {
            config.data = v;
            graph->SetDirty();
        }
    }
    else if (config.type == fg::ConfigurableType::Float)
    {
        float v = std::any_cast<float>(config.data);
        if (ImGui::DragFloat("", &v, 0.1f, 0.0f, 0.0f, "%.7f", ImGuiSliderFlags_NoRoundToFormat))
        {
            config.data = v;
            graph->SetDirty();
        }
    }
    else if (config.type == fg::ConfigurableType::Int)
    {
        int v = std::any_cast<int>(config.data);
        if (ImGui::InputInt("", &v))
        {
            config.data = v;
            graph->SetDirty();
        }
    }
    else if (config.type == fg::ConfigurableType::Vec2)
    {
        glm::vec2 v = std::any_cast<glm::vec2>(config.data);
        if (ImGui::DragFloat2("", &v[0], 0.1f, 0.f, 0.f, "%.7f", ImGuiSliderFlags_NoRoundToFormat))
        {
            config.data = v;
            graph->SetDirty();
        }
    }
    else if (config.type == fg::ConfigurableType::Vec3)
    {
        glm::vec3 v = std::any_cast<glm::vec3>(config.data);
        if (ImGui::DragFloat3("", &v[0], 0.1f, 0.f, 0.f, "%.7f", ImGuiSliderFlags_NoRoundToFormat))
        {
            config.data = v;
            graph->SetDirty();
        }
    }
    else if (config.type == fg::ConfigurableType::Vec4)
    {
        glm::vec4 v = std::any_cast<glm::vec4>(config.data);
        if (ImGui::DragFloat4("", &v[0], 0.1f, 0.f, 0.f, "%.7f", ImGuiSliderFlags_NoRoundToFormat))
        {
            config.data = v;
            graph->SetDirty();
        }
    }
    else if (config.type == fg::ConfigurableType::Vec2Int)
    {
        glm::ivec2 v = std::any_cast<glm::ivec2>(config.data);
        if (ImGui::InputInt2("", &v[0]))
        {
            config.data = v;
            graph->SetDirty();
        }
    }
    else if (config.type == fg::ConfigurableType::Vec3Int)
    {
        glm::ivec3 v = std::any_cast<glm::ivec3>(config.data);
        if (ImGui::InputInt3("", &v[0]))
        {
            config.data = v;
            graph->SetDirty();
        }
    }
    else if (config.type == fg::ConfigurableType::Vec4Int)
    {
        glm::ivec4 v = std::any_cast<glm::ivec4>(config.data);
        if (ImGui::InputInt4("", &v[0]))
        {
            config.data = v;
            graph->SetDirty();
        }
    }
    else if (config.type == fg::ConfigurableType::Format)
    {
        Gfx::ImageFormat v = std::any_cast<Gfx::ImageFormat>(config.data);

        if (ImGui::Button(Gfx::MapImageFormatToString(v)))
        {
            openImageFormatPopup = true;
            targetConfig = &config;
            graph->SetDirty();
        }
    }
    else if (config.type == fg::ConfigurableType::ObjectPtr)
    {
        Object* v = std::any_cast<Object*>(config.data);
        Asset* asAsset = dynamic_cast<Asset*>(v);

        const char* name = "empty";
        if (asAsset != nullptr)
        {
            name = asAsset->GetName().c_str();
        }
        else if (v != nullptr)
        {
            name = v->GetUUID().ToString().c_str();
        }

        if (ImGui::Button(name))
        {
            if (v != nullptr)
                EditorState::selectedObject = v->GetSRef();
        }

        if (ImGui::BeginDragDropTarget())
        {
            auto payload = ImGui::AcceptDragDropPayload("object");
            if (payload && payload->IsDelivery())
            {
                config.data = *(Object**)payload->Data;

                graph->SetDirty();
            }
            ImGui::EndDragDropTarget();
        }
    }
}
void FrameGraphEditor::Draw(ax::NodeEditor::EditorContext* context, FrameGraph::Graph& graph_)
{
    this->graph = &graph_;

    if (ImGui::Button("Compile"))
    {
        graph->Compile();
    }

    ImGui::SameLine();
    if (ImGui::Button("Validation Check"))
    {
        graph->ReportValidation();
    }

    auto overlayCursor = ImGui::GetCursorPos();

    ed::SetCurrentEditor(context);
    auto cursorPosition = ImGui::GetCursorScreenPos();

    ed::Begin("Frame Graph");
    {
        bool openImageFormatPopup = false;
        static const FrameGraph::Configurable* targetConfig = nullptr;
        if (graph)
        {
            int nodePushID = 0;
            for (std::unique_ptr<FrameGraph::Node>& node : graph->GetNodes())
            {
                nodePushID += 1;
                ImGui::PushID(nodePushID);

                ed::BeginNode(node->GetID());

                ImGui::Indent(50);
                ImGui::PushItemWidth(280);
                ImGui::Text("Node Type: %s", node->GetName().c_str());
                char customName[1024];
                strcpy(customName, node->GetCustomName().c_str());
                ImGui::Text("name");
                if (ImGui::InputText("", customName, 1024))
                {
                    node->SetCustomName(customName);
                }

                // configurable block
                int pushID = 0;

                for (auto& config : node->GetConfiurables())
                {
                    ImGui::Text("%s", config.name.c_str());
                    ImGui::PushID(pushID);
                    DrawConfigurableField(openImageFormatPopup, targetConfig, config);
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
                ImGui::PopID();
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
        }
        ed::EndCreate();

        // make links
        for (auto c : graph->GetConnections())
        {
            fg::FGID srcPropID = fg::Graph::GetSrcPropertyIDFromConnectionID(c);
            fg::FGID dstPropID = fg::Graph::GetDstPropertyIDFromConnectionID(c);
            ed::Link(c, srcPropID, dstPropID);
        }

        ed::Suspend();
        if (openImageFormatPopup)
        {
            ImGui::OpenPopup("Image Format");
        }

        if (ImGui::BeginPopup("Image Format"))
        {
            assert(targetConfig != nullptr);
            Gfx::ImageFormat v = std::any_cast<Gfx::ImageFormat>(targetConfig->data);

            for (int i = 0; i <= static_cast<int>(Gfx::ImageFormat::Invalid); ++i)
            {
                const bool isSelected = (static_cast<int>(v) == i);
                if (ImGui::MenuItem(Gfx::MapImageFormatToString(static_cast<Gfx::ImageFormat>(i))))
                {
                    targetConfig->data = static_cast<Gfx::ImageFormat>(i);
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndPopup();
        }
        ed::Resume();
    }

    ImGui::SetCursorScreenPos(cursorPosition);

    if (ed::ShowBackgroundContextMenu())
    {
        ed::Suspend();
        ImGui::OpenPopup("Create New Node");
        ed::Resume();
    }

    bool showNodeContextMenu = ed::ShowNodeContextMenu(&nodeContext);
    if (showNodeContextMenu)
    {
        ed::Suspend();
        ImGui::OpenPopup("Node Context");
        ed::Resume();
    }

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

    // deletion
    ed::Suspend();
    if (ImGui::BeginPopup("Node Context"))
    {
        fg::FGID id = nodeContext.Get();

        fg::Node* n = graph->GetNode(id);
        if (n->GetObjectTypeID() == FrameGraph::ImageNode::StaticGetObjectTypeID())
        {
            if (ImGui::MenuItem("Set As Output Image"))
            {
                graph->SetOutputImageNode(id);
            }
        }

        if (ImGui::MenuItem("Delete"))
        {
            ed::DeleteNode(id);
            graph->DeleteNode(id);
        }

        ImGui::EndPopup();
    }
    ed::Resume();

    if (ImGui::IsKeyDown(ImGuiKey_Delete))
    {
        int count = ed::GetSelectedObjectCount();
        std::vector<ed::LinkId> links(count);
        ed::GetSelectedLinks(links.data(), count);
        for (int i = 0; i < count; ++i)
        {
            graph->DeleteConnection(links[i].Get());
        }
    }

    ed::End();
    ed::SetCurrentEditor(nullptr);

    // the graph editor blocks button interaction so we can't overlay the button on top of the graph
    ImGui::SetCursorPos(overlayCursor);

    const char* outputImageName = "null";
    if (fg::Node* outputImageNode = graph->GetOutputImageNode())
    {
        outputImageName = outputImageNode->GetCustomName().c_str();
    }
    ImGui::Text("Output Image: %s", outputImageName);
}

void FrameGraphEditor::DrawFloatProp(FrameGraph::Property& p) {}

void FrameGraphEditor::DrawImageProp(FrameGraph::Property& p)
{
    // ImGui::Text("%s", p.GetName().c_str());
}

void FrameGraphEditor::DrawProperty(FrameGraph::Property& p, ax::NodeEditor::PinKind kind)
{
    if (kind == ed::PinKind::Output)
        ImGui::Indent(330);
    ed::BeginPin(p.GetID(), kind);
    ImGui::Text("%s", p.GetName().c_str());
    ed::EndPin();
    if (kind == ed::PinKind::Output)
        ImGui::Unindent(330);
}
} // namespace Editor
