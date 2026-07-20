#include "TerrainPaintWindow.hpp"

#include "Editor/GameEditor.hpp"
#include "Editor/Tools/TerrainPaintTool.hpp"
#include "Engine/ThirdParty/imgui/imgui.h"
#include <algorithm>

namespace Editor
{
DEFINE_EDITOR_WINDOW(TerrainPaintWindow, "Tools/Terrain Paint")

void TerrainPaintWindow::SetTargetTerrain(Terrain* terrain)
{
    if (tool == nullptr)
        tool = std::make_unique<TerrainPaintTool>();
    tool->SetTargetTerrain(terrain);
}

void TerrainPaintWindow::OnClose()
{
    if (isActive && GameEditor::instance != nullptr)
        GameEditor::instance->SetActiveSceneEditorTool(nullptr);
    isActive = false;
}

bool TerrainPaintWindow::Tick()
{
    bool open = true;
    if (ImGui::Begin("Terrain Paint", &open))
    {
        if (tool == nullptr)
            tool = std::make_unique<TerrainPaintTool>();

        const bool hasTarget = tool->HasTarget();
        if (hasTarget)
            ImGui::Text("Target: %s", tool->GetTargetName().c_str());
        else
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f), "No paintable Terrain connected.");
        ImGui::Text("Status: %s", isActive ? "Active" : "Inactive");
        ImGui::Separator();

        if (hasTarget)
        {
            int brushType = static_cast<int>(tool->GetBrushType());
            if (ImGui::Combo("Brush Type", &brushType, "Height\0Smooth\0Layer\0"))
                tool->SetBrushType(static_cast<TerrainPaintBrushType>(brushType));
            ImGui::SliderFloat("Brush Radius", &tool->brushRadius, 0.1f, 50.0f, "%.2f m");
            if (tool->GetBrushType() == TerrainPaintBrushType::Height)
                ImGui::SliderFloat("Height Speed", &tool->brushStrength, 0.1f, 50.0f, "%.2f m/s");
            else if (tool->GetBrushType() == TerrainPaintBrushType::Smooth)
                ImGui::SliderFloat("Smooth Speed", &tool->smoothStrength, 0.1f, 20.0f, "%.2f /s");
            else
                ImGui::SliderFloat("Layer Speed", &tool->layerStrength, 0.1f, 20.0f, "%.2f /s");
            ImGui::SliderFloat("Falloff", &tool->brushFalloff, 0.25f, 8.0f, "%.2f");

            if (tool->GetBrushType() == TerrainPaintBrushType::Layer)
            {
                TerrainConfig* config = tool->GetTargetTerrain()->GetTerrainConfig();
                const std::vector<TerrainLayer>& layers = config->GetLayers();
                auto selectedLayer = std::find_if(layers.begin(), layers.end(), [this](const TerrainLayer& layer)
                {
                    return layer.id == tool->GetSelectedLayerID();
                });
                if (selectedLayer == layers.end() && !layers.empty())
                {
                    tool->SetSelectedLayerID(layers.front().id);
                    selectedLayer = layers.begin();
                }

                const std::string preview = selectedLayer != layers.end()
                                                ? selectedLayer->name + " (ID " + std::to_string(selectedLayer->id) + ")"
                                                : "None";
                if (ImGui::BeginCombo("Terrain Layer", preview.c_str()))
                {
                    for (const TerrainLayer& layer : layers)
                    {
                        const bool selected = layer.id == tool->GetSelectedLayerID();
                        const std::string label = layer.name + " (ID " + std::to_string(layer.id) + ")";
                        if (ImGui::Selectable(label.c_str(), selected))
                            tool->SetSelectedLayerID(layer.id);
                        if (selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                if (!config->HasValidLayerControlData())
                {
                    ImGui::TextColored(
                        ImVec4(1.0f, 0.4f, 0.3f, 1.0f),
                        "Assign and initialize Layer Controls in the TerrainConfig inspector."
                    );
                }
                else if (layers.empty())
                {
                    ImGui::TextColored(
                        ImVec4(1.0f, 0.4f, 0.3f, 1.0f),
                        "Add a terrain layer in the TerrainConfig inspector."
                    );
                }
                else if (!tool->CanPaintSelectedLayer())
                {
                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f), "The selected layer no longer exists.");
                }
            }

            if (tool->GetBrushType() == TerrainPaintBrushType::Height)
            {
                ImGui::TextDisabled("Left mouse: raise terrain");
                ImGui::TextDisabled("Shift + left mouse: lower terrain");
            }
            else if (tool->GetBrushType() == TerrainPaintBrushType::Smooth)
            {
                ImGui::TextDisabled("Left mouse: smooth terrain");
            }
            else
            {
                ImGui::TextDisabled("Left mouse: gradually replace with the selected layer");
                ImGui::TextDisabled("Shift has no effect for layer painting");
            }
        }
        else
        {
            ImGui::TextDisabled("Open this window from a Terrain component with valid height data.");
        }

        ImGui::Separator();
        if (isActive)
        {
            if (ImGui::Button("Deactivate"))
            {
                if (GameEditor::instance != nullptr)
                    GameEditor::instance->SetActiveSceneEditorTool(nullptr);
                isActive = false;
            }
        }
        else
        {
            const bool canActivate = hasTarget &&
                                     (tool->GetBrushType() != TerrainPaintBrushType::Layer ||
                                      tool->CanPaintSelectedLayer());
            if (!canActivate)
                ImGui::BeginDisabled();
            if (ImGui::Button("Activate"))
            {
                if (GameEditor::instance != nullptr)
                    GameEditor::instance->SetActiveSceneEditorTool(tool.get());
                isActive = true;
            }
            if (!canActivate)
                ImGui::EndDisabled();
        }
    }
    ImGui::End();
    return open;
}
} // namespace Editor
