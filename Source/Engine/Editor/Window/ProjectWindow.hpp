#pragma once

#include "../EditorContext.hpp"
#include "../ProjectManagement/ProjectManagement.hpp"
namespace Engine::Editor
{
class ProjectWindow
{
public:
    ProjectWindow(RefPtr<EditorContext> editorContext, RefPtr<ProjectManagement> projectManagement);
    void Tick(bool& open);

private:
    RefPtr<ProjectManagement> projectManagement;
    RefPtr<EditorContext> editorContext;

    void DrawNotInitializedWindow();
    void DrawInitializedWindow();
    char projectPath[256] = {0};
};
} // namespace Engine::Editor
