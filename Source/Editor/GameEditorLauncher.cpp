#include "Editor/GameEditorLauncher.hpp"
#include "Editor/GameEditor.hpp"

void LaunchGameEditor(const char* projectPath)
{
    auto editor = std::make_unique<Editor::GameEditor>(projectPath);
    editor->Start();
}
