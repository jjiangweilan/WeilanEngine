#include "ProjectManagementWindow.hpp"
#include "ThirdParty/imgui/imgui.h"

namespace Engine::Editor
{
ProjectManagementWindow::ProjectManagementWindow(RefPtr<EditorContext> editorContext,
                                                 RefPtr<ProjectManagement> projectManagement)
    : editorContext(editorContext), projectManagement(projectManagement)
{
    state = WindowState::LoadLastProject;
}

void ProjectManagementWindow::Tick()
{
    auto& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                            ImGuiCond_FirstUseEver,
                            ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize({650, 500}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Projects", nullptr, ImGuiWindowFlags_MenuBar);
    ImGui::BeginMenuBar();
    if (ImGui::MenuItem("Create New Project"))
        state = WindowState::CreateNewProject;
    // if (ImGui::MenuItem("Last Project")) state = WindowState::LoadLastProject;
    ImGui::EndMenuBar();

    switch (state)
    {
        case WindowState::CreateNewProject: CreateNewProject(); break;
        case WindowState::LoadLastProject: LoadLastProject(); break;
    }
    ImGui::End();
}

void ProjectManagementWindow::LoadLastProject()
{
    auto list = projectManagement->GetProjectLists();
    for (auto& p : list)
    {
        ImGui::Text("%s", p.string().c_str());
        ImGui::SameLine();
        if (ImGui::Button("Load"))
        {
            projectManagement->LoadProject(p);
            ImGui::LoadIniSettingsFromDisk("imgui.ini");
        }
    }
}

void ProjectManagementWindow::CreateNewProject()
{
    ImGui::Text("%s", "Enter Project Path: ");
    ImGui::SameLine();
    ImGui::InputText("##InputText", projectPath, 256);
    ImGui::SameLine();
    if (ImGui::Button("Confirm"))
    {
        projectManagement->CreateNewProject(projectPath);
        ImGui::LoadIniSettingsFromDisk("imgui.ini");
    }
    if (ImGui::Button("Clear"))
    {
        projectPath[0] = '\0';
    }
}
} // namespace Engine::Editor
