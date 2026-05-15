#include "GrassSurfacePaintWindow.hpp"
#include "Editor/GameEditor.hpp"
#include "Editor/SceneEditor.hpp"
#include "Editor/SceneEditorTool.hpp"
#include "Editor/Tools/GrassSurfacePaintTool.hpp"
#include "Engine/Runtime/Object/Component/GrassSurface.hpp"
#include "Engine/ThirdParty/imgui/imgui.h"

namespace Editor
{

DEFINE_EDITOR_WINDOW(GrassSurfacePaintWindow, "Tools/Grass Paint")

void GrassSurfacePaintWindow::SetTargetGrassSurface(GrassSurface* gs)
{
    if (!tool)
        tool = std::make_unique<GrassSurfacePaintTool>();
    tool->SetTargetGrassSurface(gs);
}

void GrassSurfacePaintWindow::OnClose()
{
    if (isActive && GameEditor::instance)
        GameEditor::instance->SetActiveSceneEditorTool(nullptr);
    isActive = false;
}

bool GrassSurfacePaintWindow::Tick()
{
    bool open = true;
    if (ImGui::Begin("Grass Paint", &open))
    {
        if (!tool)
            tool = std::make_unique<GrassSurfacePaintTool>();

        bool hasTarget = tool->HasTarget();
        if (hasTarget)
            ImGui::Text("Target: %s", tool->GetTargetName().c_str());
        else
            ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "No target GrassSurface connected.");

        ImGui::Text("Status: %s", isActive ? "Active" : "Inactive");
        ImGui::Text("Patches: %d", tool->GetPatchCount());
        ImGui::Separator();

        if (hasTarget)
        {
            ImGui::SliderFloat("Brush Radius", &tool->brushRadius, 0.1f, 10.0f, "%.2f");
            ImGui::SliderFloat("Spacing", &tool->spacing, 0.05f, 5.0f, "%.2f");
            ImGui::SliderInt("Density", &tool->density, 1, 20);
            ImGui::Separator();
            ImGui::Checkbox("Match Center Normal", &tool->matchCenterNormal);
            if (tool->matchCenterNormal)
            {
                ImGui::Indent();
                ImGui::SliderFloat("Normal Tolerance", &tool->normalToleranceAngle, 5.0f, 90.0f, "%.0f deg");
                ImGui::Unindent();
            }
            ImGui::Checkbox("Erase Mode", &tool->eraseMode);
            ImGui::TextDisabled("Configure patch meshes in the GrassSurface component inspector.");
        }
        else
        {
            ImGui::TextDisabled("Open this tool from a GrassSurface inspector to connect a target.");
        }

        ImGui::Separator();
        if (isActive)
        {
            if (ImGui::Button("Deactivate"))
            {
                if (GameEditor::instance)
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
                if (GameEditor::instance)
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