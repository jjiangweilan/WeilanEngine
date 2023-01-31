#include "ProjectWindow.hpp"
#include "ThirdParty/imgui/imgui.h"

namespace Engine::Editor
{
ProjectWindow::ProjectWindow(RefPtr<EditorContext> editorContext, RefPtr<ProjectManagement> projectManagement)
    : projectManagement(projectManagement), editorContext(editorContext)
{}

void ProjectWindow::Tick(bool& open)
{
    ImGui::Begin("Project", &open, ImGuiWindowFlags_AlwaysAutoResize);
    if (open)
    {
        if (!projectManagement->IsInitialized())
            DrawNotInitializedWindow();
        else
            DrawInitializedWindow();
    }
    ImGui::End();
}

void ProjectWindow::DrawNotInitializedWindow()
{
    ImGui::Text("%s", "Enter Project Path: ");
    ImGui::SameLine();
    ImGui::InputText("##InputText", projectPath, 256);
    ImGui::SameLine();
    if (ImGui::Button("Confirm"))
    {
        projectManagement->CreateNewProject(projectPath);
    }
    if (ImGui::Button("Clear"))
    {
        projectPath[0] = '\0';
    }
}

void ProjectWindow::DrawInitializedWindow() {}
} // namespace Engine::Editor
