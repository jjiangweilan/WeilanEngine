#include "GrassSurfacePaintWindow.hpp"
#include "Editor/GameEditor.hpp"
#include "Editor/SceneEditor.hpp"
#include "Editor/SceneEditorTool.hpp"
#include "Editor/Tools/GrassSurfacePaintTool.hpp"
#include "Engine/ThirdParty/imgui/imgui.h"

namespace Editor
{

DEFINE_EDITOR_WINDOW(GrassSurfacePaintWindow, "Tools/Grass Paint")

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
        {
            tool = std::make_unique<GrassSurfacePaintTool>();
        }

        ImGui::Text("Status: %s", isActive ? "Active" : "Inactive");
        ImGui::Text("Target: %s", tool->GetTargetName().c_str());
        ImGui::Text("Patches: %d", tool->GetPatchCount());
        ImGui::Separator();

        ImGui::SliderFloat("Brush Radius", &tool->brushRadius, 0.1f, 10.0f, "%.2f");
        ImGui::SliderFloat("Spacing", &tool->spacing, 0.05f, 5.0f, "%.2f");
        ImGui::SliderInt("Density", &tool->density, 1, 20);
        ImGui::Checkbox("Erase Mode", &tool->eraseMode);
        ImGui::TextDisabled("Configure patch meshes in the GrassSurface component inspector.");

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
            if (ImGui::Button("Activate"))
            {
                if (GameEditor::instance)
                    GameEditor::instance->SetActiveSceneEditorTool(tool.get());
                isActive = true;
            }
        }
    }
    ImGui::End();
    return open;
}

} // namespace Editor
