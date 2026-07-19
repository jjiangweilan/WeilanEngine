#include "TerrainPaintWindow.hpp"

#include "Editor/GameEditor.hpp"
#include "Editor/Tools/TerrainPaintTool.hpp"
#include "Engine/ThirdParty/imgui/imgui.h"

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
            int brushType = static_cast<int>(tool->brushType);
            if (ImGui::Combo("Brush Type", &brushType, "Height\0Smooth\0"))
                tool->brushType = static_cast<TerrainPaintBrushType>(brushType);
            ImGui::SliderFloat("Brush Radius", &tool->brushRadius, 0.1f, 50.0f, "%.2f m");
            if (tool->brushType == TerrainPaintBrushType::Height)
                ImGui::SliderFloat("Height Speed", &tool->brushStrength, 0.1f, 50.0f, "%.2f m/s");
            else
                ImGui::SliderFloat("Smooth Speed", &tool->smoothStrength, 0.1f, 20.0f, "%.2f /s");
            ImGui::SliderFloat("Falloff", &tool->brushFalloff, 0.25f, 8.0f, "%.2f");
            if (tool->brushType == TerrainPaintBrushType::Height)
            {
                ImGui::TextDisabled("Left mouse: raise terrain");
                ImGui::TextDisabled("Shift + left mouse: lower terrain");
            }
            else
            {
                ImGui::TextDisabled("Left mouse: smooth terrain");
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
            if (!hasTarget)
                ImGui::BeginDisabled();
            if (ImGui::Button("Activate"))
            {
                if (GameEditor::instance != nullptr)
                    GameEditor::instance->SetActiveSceneEditorTool(tool.get());
                isActive = true;
            }
            if (!hasTarget)
                ImGui::EndDisabled();
        }
    }
    ImGui::End();
    return open;
}
} // namespace Editor
