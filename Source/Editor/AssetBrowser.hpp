#pragma once
#include "GfxDriver/GfxDriver.hpp"
#include "Libs/Math.hpp"
#include <filesystem>
#include <functional>
#include <unordered_map>

class WeilanEngine;

namespace Editor
{
class GameEditor;

class AssetIcon
{
public:
    Gfx::Image* GetImage();
};

class AssetBrowser
{
public:
    AssetBrowser(WeilanEngine* engine, GameEditor* gameEditor);
    ~AssetBrowser() = default;

    void Show(bool& isOpen);

private:
    enum class Mode
    {
        Tree,
        Icon
    } mode = Mode::Tree;

    WeilanEngine* engine;
    GameEditor* gameEditor;
    int currentDragDropAssetFileDepth = 0;
    /**
     * @brief when mod == Mode::Icon, this is the current directory showing
     */
    std::filesystem::path currentDirectory;

    void ShowDir(const std::filesystem::path& path, int depth);
    void ShowDirUsingIcon(const std::filesystem::path& path, int depth);
    void ShowInternalAssets();
    void ShowAssetIconWithName(
        const std::filesystem::path& name,
        AssetIcon* icon,
        int2 iconSize,
        std::function<void()> onClick = nullptr,
        std::function<void()> onRightClick = nullptr
    );

    void ChangeCurrentDirectory();
    std::filesystem::path GetCurrentDirectory();

    AssetIcon* GetEditorAssetIcon(const std::filesystem::path& path);
    int2 GetIconSize();
};
} // namespace Editor
