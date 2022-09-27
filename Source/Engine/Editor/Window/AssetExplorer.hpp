#pragma once
#include <filesystem>
#include "../EditorContext.hpp"
#include "GfxDriver/GfxDriver.hpp"

namespace Engine::Editor
{
    class AssetExplorer
    {
        public:
            AssetExplorer(RefPtr<EditorContext> editorContext);
            void Tick();

        private:
            RefPtr<EditorContext> editorContext;
            void Refresh();
            void ShowDirectory(const std::filesystem::path& path);

    };
}
