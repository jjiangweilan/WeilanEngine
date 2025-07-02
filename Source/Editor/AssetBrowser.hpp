#pragma once
#include <filesystem>

class WeilanEngine;

namespace Editor
{
    class GameEditor;
    
    class AssetBrowser
    {
    public:
        AssetBrowser(WeilanEngine* engine, GameEditor* gameEditor);
        ~AssetBrowser() = default;

        void Show(bool& isOpen);

    private:
        WeilanEngine* engine;
        GameEditor* gameEditor;
        int currentDragDropAssetFileDepth = 0;

        void ShowDir(const std::filesystem::path& path, int depth);
        void ShowInternalAssets();
    };
}
