#pragma once
#include "../EditorContext.hpp"
#include "../ProjectManagement/ProjectManagement.hpp"

namespace Engine::Editor
{
class ProjectManagementWindow
{
public:
    ProjectManagementWindow(RefPtr<EditorContext> editorContext, RefPtr<ProjectManagement> projectManagement);
    void Tick();

private:
    enum class WindowState
    {
        CreateNewProject,
        LoadLastProject
    };

    RefPtr<EditorContext> editorContext;
    RefPtr<ProjectManagement> projectManagement;
    char projectPath[256] = {0};
    WindowState state;

    void CreateNewProject();
    void LoadLastProject();
};
} // namespace Engine::Editor
