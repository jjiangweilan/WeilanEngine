#pragma once
#include "GfxDriver/GfxDriver.hpp"
#include "Libs/Math.hpp"
#include <filesystem>
#include <functional>
#include <unordered_map>
#include <vector>

class WeilanEngine;

namespace Editor
{
// Forward declarations
class GameEditor;

class AssetIcon
{
public:
    Gfx::Image* GetImage();

private:
    Gfx::Image* image;
};

class AssetBrowser
{
public:
    AssetBrowser(WeilanEngine* engine, GameEditor* gameEditor);
    ~AssetBrowser() = default;

    void Show(bool& isOpen);
    const char* GetWindowName() { return "Asset Browser"; }

private:
    enum class Mode
    {
        Tree,
        Icon
    } mode = Mode::Tree;

    WeilanEngine* engine;
    GameEditor* gameEditor;
    int currentDragDropAssetFileDepth = 0;
    std::filesystem::path lastSelectedPath = "";

    // Change File Name //
    bool changeFileName = false;
    std::filesystem::path changeFileNameTarget;
    char fileNameCache[256];
    std::string fileNameExtCache;
    void ShowChangeFileNameField();
    void ActivateFileNameField(const std::filesystem::path& path);

    // Icon size control
    float iconSizeSlider = 64.0f; // Default icon size
    static constexpr float MIN_ICON_SIZE = 0.0f;
    static constexpr float MAX_ICON_SIZE = 128.0f;
    static constexpr float TREE_MODE_THRESHOLD = 20.0f; // Switch to tree mode when below this value

    /**
     * @brief when mod == Mode::Icon, this is the current directory showing
     */
    std::filesystem::path currentDirectory;

    void ShowDir(const std::filesystem::path& path, int depth);
    void ShowDirUsingIcon(const std::filesystem::path& path, int depth);
    void ShowInternalAssets();
    void ShowIconSizeSlider(); // New method for the slider
    /**
     * @brief draw the icon using ImGUI, propagate user interaction using callbacks
     *
     * @param name name of the asset
     * @param icon icon to display
     * @param iconSize icon gui size
     * @param onClick on left click callback
     * @param onRightClick right click callback
     */
    void ShowAssetIconWithName(
        const std::filesystem::path& name,
        AssetIcon* icon,
        int2 iconSize,
        std::function<void()> onLeftClick = nullptr,
        std::function<void()> onRightClick = nullptr
    );

    void UpdateLastSelection(const std::filesystem::path& path) { this->lastSelectedPath = path; }

    /**
     * @brief Helper function to show individual asset icon items in grid layout
     */
    void ShowAssetIconItem(
        const std::filesystem::directory_entry& entry,
        float iconSize,
        int& currentColumn,
        int itemsPerRow,
        bool isDirectory
    );

    // void ChangeCurrentDirectory();
    std::filesystem::path GetCurrentDirectory();

    AssetIcon* GetEditorAssetIcon(const std::filesystem::path& path);

    int2 GetIconSize();
    void ShowMenuBar();
};
} // namespace Editor
