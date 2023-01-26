#pragma once
#include "../EditorContext.hpp"
#include "Core/AssetDatabase/DirectoryNode.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include <filesystem>

namespace Engine::Editor
{
class AssetExplorer
{
public:
    AssetExplorer(RefPtr<EditorContext> editorContext);
    void Tick();

private:
    RefPtr<EditorContext> editorContext;
    void ShowDirectory(const std::filesystem::path& path);

    std::vector<std::filesystem::path> directories;
    std::vector<std::filesystem::path> files;
};
} // namespace Engine::Editor
