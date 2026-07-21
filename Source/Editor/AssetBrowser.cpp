#include "Editor/AssetBrowser.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetDatabase.hpp"
#include "Engine/Core/Asset.hpp"
#include "Engine/Runtime/Object/GameObject/GameObject.hpp"
#include "Engine/Runtime/System/Navigation/NavData.hpp"
#include "Editor/EditorGUI.hpp"
#include "Editor/EditorState.hpp"
#include "Editor/FileIcons.hpp"
#include "Editor/GameEditor.hpp"
#include "Editor/NavDataAssetUtility.hpp"
#include "Editor/TerrainConfigAssetUtility.hpp"
#include "Editor/Windows/CursorAtlasEditorWindow.hpp"
#include "Engine/Library/Platform/FileExplore.hpp"
#include "Engine/Core/BinaryAsset.hpp"
#include "Engine/ThirdParty/imgui/imgui.h"
#include "Engine/WeilanEngine.hpp"
#include "Engine/Library/Utils.hpp"
#include <algorithm>
#include <vector>

namespace Editor
{

namespace
{
bool IsMetaFile(const std::filesystem::path& path)
{
    return path.extension() == ".meta";
}

void ReimportFolderRecursively(const std::filesystem::path& folder)
{
    for (const auto& entry : std::filesystem::recursive_directory_iterator(folder))
    {
        if (!entry.is_regular_file() || IsMetaFile(entry.path()))
        {
            continue;
        }

        AssetPath assetPath(entry.path());
        if (AssetDatabase::Singleton()->CanImport(assetPath))
        {
            AssetDatabase::Singleton()->Reimport(assetPath);
        }
    }
}
} // namespace

AssetBrowser::AssetBrowser(WeilanEngine* engine, GameEditor* gameEditor)
    : engine(engine), gameEditor(gameEditor), currentDragDropAssetFileDepth(0)
{
    currentDirectory = engine->GetProjectAssetPath();
}

bool AssetBrowser::IsSelected(const AssetPath& path) const
{
    return std::find(selectedPaths.begin(), selectedPaths.end(), path) != selectedPaths.end();
}

void AssetBrowser::ClearSelection()
{
    selectedPaths.clear();
    activeSelectedPath = {};
}

void AssetBrowser::ReplaceSelection(const AssetPath& path)
{
    selectedPaths.clear();
    selectedPaths.push_back(path);
    activeSelectedPath = path;
}

void AssetBrowser::AddSelection(const AssetPath& path)
{
    if (!IsSelected(path))
    {
        selectedPaths.push_back(path);
    }
    activeSelectedPath = path;
}

void AssetBrowser::RemoveSelection(const AssetPath& path)
{
    auto selected = std::find(selectedPaths.begin(), selectedPaths.end(), path);
    if (selected == selectedPaths.end())
    {
        return;
    }

    selectedPaths.erase(selected);
    if (activeSelectedPath == path)
    {
        activeSelectedPath = selectedPaths.empty() ? AssetPath{} : selectedPaths.back();
    }
}

void AssetBrowser::SetActiveSelection(const AssetPath& path)
{
    if (IsSelected(path))
    {
        activeSelectedPath = path;
    }
}

void AssetBrowser::SelectActiveAssetInEditor()
{
    if (activeSelectedPath.empty() || std::filesystem::is_directory(activeSelectedPath.ToAbsolutePath()))
    {
        return;
    }

    if (Asset* asset = engine->assetDatabase->LoadAsset(activeSelectedPath))
    {
        EditorState::SelectObject(asset);
    }
}

void AssetBrowser::SelectFromClick(const AssetPath& path)
{
    const ImGuiIO& io = ImGui::GetIO();
    if (io.KeyAlt)
    {
        RemoveSelection(path);
    }
    else if (io.KeyShift)
    {
        AddSelection(path);
    }
    else
    {
        ReplaceSelection(path);
    }

    SelectActiveAssetInEditor();
}

bool AssetBrowser::MarqueeIntersects(const float2& min, const float2& max) const
{
    if (!marqueeSelection.active || !marqueeSelection.hasDragged)
    {
        return false;
    }

    const float selectionMinX = std::min(marqueeSelection.start.x, marqueeSelection.current.x);
    const float selectionMinY = std::min(marqueeSelection.start.y, marqueeSelection.current.y);
    const float selectionMaxX = std::max(marqueeSelection.start.x, marqueeSelection.current.x);
    const float selectionMaxY = std::max(marqueeSelection.start.y, marqueeSelection.current.y);
    return max.x >= selectionMinX && min.x <= selectionMaxX &&
           max.y >= selectionMinY && min.y <= selectionMaxY;
}

bool AssetBrowser::WillBeSelected(const AssetPath& path, const float2& min, const float2& max) const
{
    const bool isSelected = IsSelected(path);
    if (!marqueeSelection.active || !marqueeSelection.hasDragged)
    {
        return isSelected;
    }

    const bool intersects = MarqueeIntersects(min, max);
    const ImGuiIO& io = ImGui::GetIO();
    if (io.KeyAlt)
    {
        return isSelected && !intersects;
    }
    if (io.KeyShift)
    {
        return isSelected || intersects;
    }
    return intersects;
}

const AssetBrowser::VisibleIconItem* AssetBrowser::FindVisibleIconItem(const AssetPath& path) const
{
    auto item = std::find_if(
        visibleIconItems.begin(),
        visibleIconItems.end(),
        [&path](const VisibleIconItem& visibleItem)
        { return visibleItem.path == path; }
    );
    return item == visibleIconItems.end() ? nullptr : &*item;
}

AssetPath AssetBrowser::ResolveMarqueeActivePath() const
{
    if (!marqueeSelection.active || !marqueeSelection.hasDragged)
    {
        return activeSelectedPath;
    }

    auto willRemainSelected = [this](const AssetPath& path)
    {
        const VisibleIconItem* item = FindVisibleIconItem(path);
        return item != nullptr && WillBeSelected(path, item->min, item->max);
    };

    if (!marqueeSelection.lastHoveredPath.empty() && willRemainSelected(marqueeSelection.lastHoveredPath))
    {
        return marqueeSelection.lastHoveredPath;
    }
    if (!activeSelectedPath.empty() && willRemainSelected(activeSelectedPath))
    {
        return activeSelectedPath;
    }

    for (auto selected = selectedPaths.rbegin(); selected != selectedPaths.rend(); ++selected)
    {
        if (willRemainSelected(*selected))
        {
            return *selected;
        }
    }

    for (auto item = visibleIconItems.rbegin(); item != visibleIconItems.rend(); ++item)
    {
        if (WillBeSelected(item->path, item->min, item->max))
        {
            return item->path;
        }
    }

    return {};
}

void AssetBrowser::DrawActiveSelectionOutline() const
{
    const AssetPath activePath = ResolveMarqueeActivePath();
    const VisibleIconItem* activeItem = FindVisibleIconItem(activePath);
    if (activeItem == nullptr)
    {
        return;
    }

    ImGui::GetWindowDrawList()->AddRect(
        {activeItem->min.x, activeItem->min.y},
        {activeItem->max.x, activeItem->max.y},
        IM_COL32(60, 130, 240, 255),
        4.0f,
        0,
        2.0f
    );
}

void AssetBrowser::ApplyMarqueeSelection()
{
    const AssetPath nextActivePath = ResolveMarqueeActivePath();
    std::vector<AssetPath> intersectedPaths;
    for (const VisibleIconItem& item : visibleIconItems)
    {
        if (MarqueeIntersects(item.min, item.max))
        {
            intersectedPaths.push_back(item.path);
        }
    }

    const ImGuiIO& io = ImGui::GetIO();
    if (io.KeyAlt)
    {
        for (const AssetPath& path : intersectedPaths)
        {
            RemoveSelection(path);
        }
    }
    else
    {
        if (!io.KeyShift)
        {
            ClearSelection();
        }

        for (const AssetPath& path : intersectedPaths)
        {
            AddSelection(path);
        }
    }

    if (!nextActivePath.empty() && IsSelected(nextActivePath))
    {
        activeSelectedPath = nextActivePath;
    }
    else
    {
        activeSelectedPath = selectedPaths.empty() ? AssetPath{} : selectedPaths.back();
    }

    SelectActiveAssetInEditor();
}

void AssetBrowser::RequestReimport(const std::vector<AssetPath>& paths)
{
    if (paths.empty())
    {
        return;
    }

    const std::string prompt = paths.size() == 1
                                   ? fmt::format("Reimport '{}'?", paths.front().GetFileName())
                                   : fmt::format("Reimport {} selected items?", paths.size());
    gameEditor->endPopup.Show(
        prompt,
        [this, paths]()
        {
            gameEditor->endEvents.Register(
                [paths]()
                {
                    for (const AssetPath& path : paths)
                    {
                        const std::filesystem::path absolutePath = path.ToAbsolutePath();
                        if (std::filesystem::is_directory(absolutePath))
                        {
                            ReimportFolderRecursively(absolutePath);
                        }
                        else if (AssetDatabase::Singleton()->CanImport(path))
                        {
                            AssetDatabase::Singleton()->Reimport(path);
                        }
                    }
                }
            );
        }
    );
}

void AssetBrowser::RequestDelete(const std::vector<AssetPath>& paths)
{
    if (paths.empty())
    {
        return;
    }

    const std::string prompt = paths.size() == 1
                                   ? fmt::format("Delete '{}'?", paths.front().GetFileName())
                                   : fmt::format("Delete {} selected items?", paths.size());
    gameEditor->endPopup.Show(
        prompt,
        [this, paths]()
        {
            gameEditor->endEvents.Register(
                [this, paths]()
                {
                    for (const AssetPath& path : paths)
                    {
                        AssetDatabase::Singleton()->Remove(path);
                    }
                    ClearSelection();
                    searchSelectedPath.clear();
                }
            );
        }
    );
}

void AssetBrowser::PinAsset(const AssetPath& path)
{
    if (path.empty() || path.IsInternal())
        return;

    std::filesystem::path absolutePath = path.ToAbsolutePath();
    currentDirectory = absolutePath.parent_path();
    ReplaceSelection(path);
    searchQuery.clear();
    searchSelectedPath.clear();
}

AssetPath AssetBrowser::GetCurrentDirectory() const
{
    return AssetPath(currentDirectory);
}

void AssetBrowser::Show(bool& isOpen)
{
    if (isOpen)
    {
        ENGINE_BEGIN_PROFILE("AssetBrowser - Show")

        std::filesystem::path fullAssetsPath = engine->GetProjectPath() / "Assets";

        if (ImGui::IsKeyPressed(ImGuiKey_Escape) && !searchQuery.empty())
        {
            if (!searchSelectedPath.empty())
            {
                currentDirectory = searchSelectedPath.parent_path();
                searchSelectedPath.clear();
            }
            searchQuery.clear();
        }

        ImGui::Begin(GetWindowName(), &isOpen, ImGuiWindowFlags_MenuBar);

        ShowMenuBar();

        if (ImGui::BeginPopupContextItem("Asset Browser Context Popup"))
        {
            if (ImGui::MenuItem("Create Folder"))
            {
                AssetDatabase::Singleton()->CreateFolderAtPath(fullAssetsPath);
            }

            ImGui::EndPopup();
        }

        ShowInternalAssets();
        ImGui::Separator();

        const bool wasSearching = !searchQuery.empty();
        ShowSearchBar();
        if (!wasSearching && !searchQuery.empty())
        {
            ClearSelection();
        }

        if (!searchQuery.empty())
        {
            ShowSearchResults();
        }
        else
        {
            if (!searchSelectedPath.empty())
            {
                currentDirectory = searchSelectedPath.parent_path();
                searchSelectedPath.clear();
            }

            ShowIconSizeSlider();

            Mode effectiveMode = (iconSizeSlider <= TREE_MODE_THRESHOLD) ? Mode::Tree : Mode::Icon;

            switch (effectiveMode)
            {
                case Mode::Tree: ShowDir(fullAssetsPath, 0); break;
                case Mode::Icon: ShowDirUsingIcon(currentDirectory, 0); break;
                default: break;
            }
        }

        ImGui::End();
        ENGINE_END_PROFILE;
    }
}

void AssetBrowser::ShowInternalAssets()
{
    if (ImGui::TreeNode("_engine_internal_asset"))
    {
        auto& internalAssets = engine->assetDatabase->GetInternalAssets();
        for (AssetData* internalAsset : internalAssets)
        {
            auto path = std::filesystem::relative(internalAsset->GetAssetPath(), "_engine_internal/").string();
            if (ImGui::TreeNodeEx(path.c_str(), ImGuiTreeNodeFlags_Leaf))
            {
                if (EditorGUI::DragDropSource(
                        internalAsset->GetAssetPath().string().c_str(),
                        [internalAsset](Object*& obj)
                        {
                            Asset* asset = AssetDatabase::Singleton()->LoadAsset(internalAsset->GetAssetPath());
                            obj = asset;
                        }
                    ))
                {}
                else if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                {
                    Asset* asset = engine->assetDatabase->LoadAsset(internalAsset->GetAssetPath());
                    if (asset)
                    {
                        EditorState::SelectObject(asset);
                    }
                }

                if (ImGui::IsItemHovered())
                {
                    if (ImGui::BeginTooltip())
                    {
                        ImGui::Text("%s", path.c_str());
                        ImGui::EndTooltip();
                    }
                }
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }
}

void AssetBrowser::ShowDir(const std::filesystem::path& path, int depth)
{

    // Need access to GameEditor's deferred events.
    auto& endEvents = gameEditor->endEvents;

    for (auto entry : std::filesystem::directory_iterator(path))
    {
        if (entry.is_directory())
        {
            const std::filesystem::path& path = entry.path();
            auto relative = AssetPath(path);
            bool treeOpen = ImGui::TreeNodeEx(path.filename().string().c_str());

            if (EditorGUI::DragDropSource(relative))
            {
                currentDragDropAssetFileDepth = depth;
            }

            AssetPath pathStr;
            if (EditorGUI::DragDropTarget(pathStr))
            {
                endEvents.Register(
                    [pathStr, newDirectory = entry.path().string()]()
                    {
                        std::filesystem::path oldPath(pathStr);
                        auto newPath = newDirectory / oldPath.filename();
                        AssetDatabase::Singleton()->Rename(
                            oldPath,
                            std::filesystem::relative(newPath, AssetDatabase::Singleton()->GetAssetDirectory())
                        );
                    }
                );
            }

            Object* gameObject;
            EditorGUI::DragDropTarget(typeid(GameObject), gameObject);

            if (ImGui::BeginPopupContextItem())
            {
                if (ImGui::MenuItem("Create Folder"))
                {
                    AssetDatabase::Singleton()->CreateFolderAtPath(entry.path());
                }

                if (ImGui::MenuItem("Reimport Folder"))
                {
                    RequestReimport({AssetPath(entry.path())});
                }

                if (ImGui::MenuItem("Delete Folder"))
                {
                    RequestDelete({AssetPath(entry.path())});
                }

                if (ImGui::MenuItem("Change File Name"))
                {
                    ActivateFileNameField(entry.path());
                }
                ImGui::EndPopup();
            }
            if (treeOpen)
            {
                ShowDir(entry.path(), depth + 1);
                ImGui::TreePop();
            }
        }
    }

    if (depth == 0 && currentDragDropAssetFileDepth != 0)
    {
        auto windowPos = ImGui::GetWindowPos();
        auto currentCursor = ImGui::GetCursorPos() + windowPos - ImVec2{ImGui::GetScrollX(), ImGui::GetScrollY()};
        auto contextRegionMax = windowPos + ImVec2{ImGui::GetWindowWidth(), ImGui::GetWindowHeight()};
        AssetPath pathStr;
        if (EditorGUI::DragDropTarget(pathStr, {currentCursor, contextRegionMax}))
        {
            endEvents.Register(
                [pathStr]()
                {
                    std::filesystem::path oldPath(pathStr);
                    auto newPath = AssetDatabase::Singleton()->GetAssetDirectory() / oldPath.filename();
                    AssetDatabase::Singleton()->Rename(
                        std::filesystem::relative(oldPath, AssetDatabase::Singleton()->GetAssetDirectory()),
                        std::filesystem::relative(newPath, AssetDatabase::Singleton()->GetAssetDirectory())
                    );
                }
            );
        }
    }

    for (auto entry : std::filesystem::directory_iterator(path))
    {
        if (entry.is_regular_file())
        {
            if (IsMetaFile(entry.path()))
            {
                continue;
            }

            std::string pathStr = entry.path().filename().string();
            auto treeTitle = fmt::format("{} {}", FileIcons::GetIcon(entry.path().extension()), pathStr);
            bool open = ImGui::TreeNodeEx(treeTitle.c_str(), ImGuiTreeNodeFlags_Leaf);
            if (ImGui::BeginPopupContextItem("asset window context menu"))
            {
                AssetPath assetPath(entry.path());
                if (AssetDatabase::Singleton()->CanImport(assetPath) && ImGui::MenuItem("Reimport"))
                {
                    RequestReimport({assetPath});
                }

                if (ImGui::MenuItem("Change File Name"))
                {
                    ActivateFileNameField(entry.path());
                }

                if (ImGui::MenuItem("Delete"))
                {
                    RequestDelete({AssetPath(entry.path())});
                }
                ImGui::EndPopup();
            }

            if (open)
            {
                std::filesystem::path path = entry.path().string();
                path = AssetPath(path);
                EditorGUI::DragDropSource(
                    path,
                    [path](Object*& obj)
                    { obj = AssetDatabase::Singleton()->LoadAsset(path); }
                );

                if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                {
                    Asset* asset = engine->assetDatabase->LoadAsset(
                        std::filesystem::relative(entry.path(), engine->assetDatabase->GetAssetDirectory())
                    );
                    if (asset)
                    {
                        EditorState::SelectObject(asset);
                    }
                }

                ImGui::TreePop();
            }
        }
    }

    ShowChangeFileNameField();
}

void AssetBrowser::ShowDirUsingIcon(const std::filesystem::path& path, int depth)
{
    const float iconSize = iconSizeSlider; // Use the slider value instead of fixed size
    const float iconPadding = 8.0f;        // Padding between icons
    const float totalItemWidth = iconSize + iconPadding * 2;
    bool openCreateMenuPopup = false;

    // Calculate how many icons fit horizontally
    ImVec2 contentRegion = ImGui::GetContentRegionAvail();
    int itemsPerRow = std::max(1, (int)(contentRegion.x / totalItemWidth));

    // Navigation breadcrumb
    if (ImGui::Button("Up") && path != engine->GetProjectAssetPath())
    {
        currentDirectory = path.parent_path();
        ClearSelection();
    }
    ImGui::SameLine();
    ImGui::Text("Current: %s", std::filesystem::relative(path, engine->GetProjectAssetPath()).string().c_str());

    ImGui::Separator();

    // Begin child region for scrolling
    if (ImGui::BeginChild("IconGrid", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar))
    {
        visibleIconItems.clear();
        const ImVec2 mousePosition = ImGui::GetMousePos();
        if (marqueeSelection.active)
        {
            marqueeSelection.current = {mousePosition.x, mousePosition.y};
            const float2 dragDelta = marqueeSelection.current - marqueeSelection.start;
            if (glm::length(dragDelta) > 4.0f)
            {
                marqueeSelection.hasDragged = true;
            }
        }

        int currentColumn = 0;

        // Collect entries (directories first, then files)
        std::vector<std::filesystem::directory_entry> directories;
        std::vector<std::filesystem::directory_entry> files;

        for (const auto& entry : std::filesystem::directory_iterator(path))
        {
            if (entry.is_directory())
                directories.push_back(entry);
            else if (entry.is_regular_file() && !IsMetaFile(entry.path()))
                files.push_back(entry);
        }

        // Sort entries alphabetically
        auto sortEntries = [](const std::filesystem::directory_entry& a, const std::filesystem::directory_entry& b)
        {
            auto& pa = a.path();
            auto& pb = b.path();

            // first compare extensions
            auto ext_a = pa.extension().string();
            auto ext_b = pb.extension().string();
            if (ext_a != ext_b)
                return ext_a < ext_b;

            // if extensions are the same, compare filenames
            return pa.filename().string() < pb.filename().string();
        };
        std::sort(directories.begin(), directories.end(), sortEntries);
        std::sort(files.begin(), files.end(), sortEntries);

        // Display directories first
        for (const auto& entry : directories)
        {
            ShowAssetIconItem(entry, iconSize, currentColumn, itemsPerRow, true);
        }

        // Display files
        for (const auto& entry : files)
        {
            ShowAssetIconItem(entry, iconSize, currentColumn, itemsPerRow, false);
        }

        const bool mouseOverItem = std::any_of(
            visibleIconItems.begin(),
            visibleIconItems.end(),
            [mousePosition](const VisibleIconItem& item)
            {
                return mousePosition.x >= item.min.x && mousePosition.x <= item.max.x &&
                       mousePosition.y >= item.min.y && mousePosition.y <= item.max.y;
            }
        );
        const ImVec2 childWindowPosition = ImGui::GetWindowPos();
        const ImVec2 childContentMin = childWindowPosition + ImGui::GetWindowContentRegionMin();
        const ImVec2 childContentMax = childWindowPosition + ImGui::GetWindowContentRegionMax();
        const bool mouseInContent = mousePosition.x >= childContentMin.x && mousePosition.x <= childContentMax.x &&
                                    mousePosition.y >= childContentMin.y && mousePosition.y <= childContentMax.y;
        const bool gridHovered = ImGui::IsWindowHovered() && mouseInContent;

        if (gridHovered && !mouseOverItem && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            openCreateMenuPopup = true;
        }

        if (!marqueeSelection.active && gridHovered && !mouseOverItem &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            marqueeSelection.active = true;
            marqueeSelection.hasDragged = false;
            marqueeSelection.start = {mousePosition.x, mousePosition.y};
            marqueeSelection.current = marqueeSelection.start;
            marqueeSelection.lastHoveredPath = {};
        }

        if (marqueeSelection.active)
        {
            if (marqueeSelection.hasDragged)
            {
                const ImVec2 selectionMin{
                    std::min(marqueeSelection.start.x, marqueeSelection.current.x),
                    std::min(marqueeSelection.start.y, marqueeSelection.current.y)
                };
                const ImVec2 selectionMax{
                    std::max(marqueeSelection.start.x, marqueeSelection.current.x),
                    std::max(marqueeSelection.start.y, marqueeSelection.current.y)
                };
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                drawList->AddRectFilled(selectionMin, selectionMax, IM_COL32(60, 130, 240, 40));
                drawList->AddRect(selectionMin, selectionMax, IM_COL32(60, 130, 240, 200), 0.0f, 0, 1.5f);
            }

            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            {
                if (marqueeSelection.hasDragged)
                {
                    ApplyMarqueeSelection();
                }
                else if (!ImGui::GetIO().KeyShift && !ImGui::GetIO().KeyAlt)
                {
                    ClearSelection();
                }

                marqueeSelection.active = false;
                marqueeSelection.hasDragged = false;
                marqueeSelection.lastHoveredPath = {};
            }
        }

        DrawActiveSelectionOutline();
    }
    ImGui::EndChild();

    if (openCreateMenuPopup)
        ImGui::OpenPopup("CreateMenu");

    // Handle right-click context menu on empty space
    if (ImGui::BeginPopupContextWindow("CreateMenu"))
    {
        if (ImGui::MenuItem("Create Folder"))
        {
            AssetDatabase::Singleton()->CreateFolderAtPath(path);
        }

        if (ImGui::MenuItem("Open System Directory"))
        {
            Platform::FileExplore::OpenFolder(path);
        }

        if (ImGui::BeginMenu("Create"))
        {
            if (ImGui::MenuItem("Material"))
            {
                auto mat = std::make_unique<Material>();
                engine->assetDatabase->SaveAsset(std::move(mat), path / "New Material");
            }
            if (ImGui::MenuItem("Render Pipeline Setting"))
            {
                auto renderPipelineSetting = std::make_unique<Rendering::RenderPipelineSetting>();
                engine->assetDatabase->SaveAsset(std::move(renderPipelineSetting), path / "New RenderPipelineSetting");
            }
            if (ImGui::MenuItem("Nav Data"))
            {
                CreateNavDataAsset(*engine->assetDatabase, path / "New NavData");
            }
            if (ImGui::MenuItem("Terrain Config"))
            {
                CreateTerrainConfigAsset(*engine->assetDatabase, path / "New Terrain");
            }
            if (ImGui::MenuItem("Binary Asset"))
            {
                engine->assetDatabase->SaveAsset(std::make_unique<BinaryAsset>(), path / "New Binary Asset");
            }
            if (ImGui::MenuItem("Cursor Atlas"))
            {
                gameEditor->activeWindows.push_back(std::make_unique<CursorAtlasEditorWindow>());
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenu();
    }

    ShowChangeFileNameField();
}

void AssetBrowser::ShowAssetIconItem(
    const std::filesystem::directory_entry& entry, float iconSize, int& currentColumn, int itemsPerRow, bool isDirectory
)
{
    // Access to GameEditor's endEvents here since we can't pass it as a parameter
    auto& endEvents = gameEditor->endEvents;

    ImGui::PushID(entry.path().string().c_str());

    // Calculate position
    if (currentColumn >= itemsPerRow)
    {
        currentColumn = 0;
    }

    if (currentColumn > 0)
    {
        ImGui::SameLine();
    }

    // Get icon
    Gfx::Image* iconImage = nullptr;

    if (isDirectory)
    {
        iconImage = FileIcons::Instance().GetDirectoryIconImage();
    }
    else
    {
        iconImage = FileIcons::Instance().GetIconImage(entry.path());
    }

    constexpr float iconLabelFontSize = 14.0f;
    constexpr float iconLabelHeight = 40.0f;
    constexpr float iconLabelGap = 4.0f;
    const ImVec2 tileSize{iconSize, iconSize + iconLabelGap + iconLabelHeight};
    const bool isClicked = ImGui::InvisibleButton("##tile", tileSize);

    const ImVec2 tileMin = ImGui::GetItemRectMin();
    const ImVec2 tileMax = ImGui::GetItemRectMax();
    ImVec2 iconMin = tileMin;
    ImVec2 iconMax{tileMin.x + iconSize, tileMin.y + iconSize};
    const bool isHovered = ImGui::IsItemHovered();
    const bool isDoubleClicked = isHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
    const bool isRightClicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);
    const AssetPath itemPath(entry.path());
    const float2 tileMinPosition{tileMin.x, tileMin.y};
    const float2 tileMaxPosition{tileMax.x, tileMax.y};
    const bool willBeSelected = WillBeSelected(itemPath, tileMinPosition, tileMaxPosition);

    visibleIconItems.push_back({itemPath, tileMinPosition, tileMaxPosition});
    if (marqueeSelection.active && marqueeSelection.hasDragged && isHovered)
    {
        marqueeSelection.lastHoveredPath = itemPath;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    if (willBeSelected)
    {
        drawList->AddRectFilled(tileMin, tileMax, IM_COL32(60, 130, 240, 90), 4.0f);
    }
    else if (isHovered)
    {
        drawList->AddRectFilled(tileMin, tileMax, IM_COL32(140, 140, 140, 70), 4.0f);
    }

    const ImU32 iconBackgroundColor = isHovered ? IM_COL32(140, 140, 140, 100) : IM_COL32(70, 70, 70, 100);
    drawList->AddRectFilled(iconMin, iconMax, iconBackgroundColor, 4.0f);

    // Draw text icon for now (until we can properly access texture system)
    if (iconImage)
    {
        iconMin.x += 4;
        iconMin.y += 4;
        iconMax.x -= 4;
        iconMax.y -= 4;
        EditorGUI::Image(*iconImage, iconMin, iconMax);
    }

    // File/folder name label
    std::string filename = entry.path().filename().string();

    const float textWidth = iconSize;
    const ImVec2 labelScreenPos{tileMin.x, tileMin.y + iconSize + iconLabelGap};
    drawList->AddText(
        ImGui::GetFont(),
        iconLabelFontSize,
        labelScreenPos,
        ImGui::GetColorU32(ImGuiCol_Text),
        filename.c_str(),
        nullptr,
        textWidth
    );

    if (isHovered)
    {
        drawList->AddRect(tileMin, tileMax, IM_COL32(180, 180, 180, 160), 4.0f, 0, 1.0f);
    }

    // Handle interactions
    if (isDoubleClicked)
    {
        if (isDirectory)
        {
            // Navigate into directory
            currentDirectory = entry.path();
            ClearSelection();
        }
        else
        {
            SetActiveSelection(itemPath);
            SelectActiveAssetInEditor();
        }
    }
    else if (isClicked)
    {
        SelectFromClick(itemPath);
    }

    // Handle drag and drop
    if (isDirectory)
    {
        auto relative = AssetPath(entry.path());
        EditorGUI::DragDropSource(relative, ImGuiDragDropFlags_SourceAllowNullID);

        AssetPath pathStr;
        if (EditorGUI::DragDropTarget(pathStr))
        {
            endEvents.Register(
                [pathStr, newDirectory = entry.path().string()]()
                {
                    std::filesystem::path oldPath(pathStr);
                    auto newPath = newDirectory / oldPath.filename();
                    AssetDatabase::Singleton()->Rename(
                        oldPath,
                        std::filesystem::relative(newPath, AssetDatabase::Singleton()->GetAssetDirectory())
                    );
                }
            );
        }
    }
    else
    {
        std::filesystem::path filePath = entry.path().string();
        filePath = AssetPath(filePath);
        EditorGUI::DragDropSource(
            filePath,
            [filePath](Object*& obj)
            { obj = AssetDatabase::Singleton()->LoadAsset(filePath); },
            ImGuiDragDropFlags_SourceAllowNullID
        );
    }

    // Context menu
    if (isRightClicked)
    {
        if (IsSelected(itemPath))
        {
            SetActiveSelection(itemPath);
        }
        else
        {
            ReplaceSelection(itemPath);
        }
        SelectActiveAssetInEditor();
        ImGui::OpenPopup("ItemContextMenu");
    }

    if (ImGui::BeginPopup("ItemContextMenu"))
    {
        const std::vector<AssetPath> contextSelection = selectedPaths;
        if (isDirectory)
        {
            if (ImGui::MenuItem("Create Folder"))
            {
                AssetDatabase::Singleton()->CreateFolderAtPath(entry.path());
            }
        }

        if (ImGui::MenuItem("Rename"))
        {
            ActivateFileNameField(activeSelectedPath);
        }

        const bool canReimport = std::any_of(
            contextSelection.begin(),
            contextSelection.end(),
            [](const AssetPath& path)
            {
                return std::filesystem::is_directory(path.ToAbsolutePath()) ||
                       AssetDatabase::Singleton()->CanImport(path);
            }
        );
        const std::string reimportLabel = contextSelection.size() > 1
                                              ? fmt::format("Reimport Selected ({})", contextSelection.size())
                                              : "Reimport";
        if (canReimport && ImGui::MenuItem(reimportLabel.c_str()))
        {
            RequestReimport(contextSelection);
        }

        const std::string deleteLabel = contextSelection.size() > 1
                                            ? fmt::format("Delete Selected ({})", contextSelection.size())
                                            : "Delete";
        if (ImGui::MenuItem(deleteLabel.c_str()))
        {
            RequestDelete(contextSelection);
        }

        ImGui::EndPopup();
    }

    currentColumn++;
    ImGui::PopID();
}

void AssetBrowser::ShowAssetIconWithName(
    const std::filesystem::path& name,
    AssetIcon* icon,
    int2 iconSize,
    std::function<void()> onLeftClick,
    std::function<void()> onRightClick
)
{
    // For now, just display a placeholder rectangle
    ImVec2 size = {(float)iconSize.x, (float)iconSize.y};
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImU32 color = IM_COL32(100, 100, 100, 255);
    ImGui::GetWindowDrawList()->AddRectFilled(pos, {pos.x + size.x, pos.y + size.y}, color);
    ImGui::Dummy(size);
}

int2 AssetBrowser::GetIconSize()
{
    return {35, 35};
}

void AssetBrowser::ShowMenuBar()
{
    ImGui::BeginMenuBar();

    // Show current mode status instead of switch button
    if (iconSizeSlider <= TREE_MODE_THRESHOLD)
    {
        ImGui::Text("Mode: Tree (Icon Size: %.0f)", iconSizeSlider);
    }
    else
    {
        ImGui::Text("Mode: Icon (Icon Size: %.0f)", iconSizeSlider);
    }

    ImGui::EndMenuBar();
}

void AssetBrowser::ShowSearchBar()
{
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 60.0f);
    EditorGUI::InputText("##Search", searchQuery, "Search Assets...");
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button("x"))
    {
        searchQuery.clear();
    }
    if (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Escape) && !searchQuery.empty())
    {
        searchQuery.clear();
    }
}

AssetIcon* AssetBrowser::GetEditorAssetIcon(const std::filesystem::path& path)
{
    static AssetIcon empty;
    return &empty;
}

void AssetBrowser::ShowIconSizeSlider()
{
    const float sliderWidth = 120.0f;
    const float sliderHeight = 20.0f;
    const float margin = 10.0f;

    // Set the local cursor position (relative to the window) for the slider

    // Create the slider with the desired width.
    ImGui::PushItemWidth(sliderWidth);
    bool sliderChanged = ImGui::SliderFloat("##IconSize", &iconSizeSlider, MIN_ICON_SIZE, MAX_ICON_SIZE, "%.0f");
    ImGui::PopItemWidth();

    // Show a tooltip when hovering the slider
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        if (iconSizeSlider <= TREE_MODE_THRESHOLD)
        {
            ImGui::Text("Tree Mode (Icon Size: %.0f)", iconSizeSlider);
        }
        else
        {
            ImGui::Text("Icon Mode (Icon Size: %.0f)", iconSizeSlider);
        }
        ImGui::EndTooltip();
    }

    // Update the display mode based on slider changes
    if (sliderChanged)
    {
        const Mode newMode = (iconSizeSlider <= TREE_MODE_THRESHOLD) ? Mode::Tree : Mode::Icon;
        if (newMode != mode)
        {
            ClearSelection();
            mode = newMode;
        }
    }
}

void AssetBrowser::PerformSearch(std::vector<std::filesystem::directory_entry>& results)
{
    results.clear();
    if (searchQuery.empty())
        return;

    std::filesystem::path assetsPath = engine->GetProjectPath() / "Assets";
    std::string lowerQuery = Utils::strToLower(searchQuery);

    for (auto entry : std::filesystem::recursive_directory_iterator(assetsPath))
    {
        if (entry.is_directory())
            continue;
        if (IsMetaFile(entry.path()))
            continue;

        std::string filename = entry.path().filename().string();
        std::string lowerFilename = Utils::strToLower(filename);
        if (Utils::strContians(lowerFilename, lowerQuery))
        {
            results.push_back(entry);
        }
    }

    auto sortEntries = [](const std::filesystem::directory_entry& a, const std::filesystem::directory_entry& b)
    {
        return a.path().filename().string() < b.path().filename().string();
    };
    std::sort(results.begin(), results.end(), sortEntries);
}

void AssetBrowser::ShowSearchResults()
{
    std::vector<std::filesystem::directory_entry> results;
    PerformSearch(results);

    if (results.empty())
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled), "No assets found");
        return;
    }

    ImGui::BeginChild("SearchResults", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    for (const auto& entry : results)
    {
        std::string filename = entry.path().filename().string();
        std::filesystem::path relativePath = std::filesystem::relative(entry.path(), engine->GetProjectPath() / "Assets");

        ImGui::PushID(entry.path().string().c_str());

        float rowHeight = 20.0f;
        ImVec2 cursorPos = ImGui::GetCursorPos();
        ImVec2 buttonSize = {ImGui::GetContentRegionAvail().x, rowHeight};

        bool isSelected = !searchSelectedPath.empty() && AssetPath(searchSelectedPath) == AssetPath(entry.path());

        ImGui::InvisibleButton("##searchrow", buttonSize);
        bool isHovered = ImGui::IsItemHovered();
        bool isClicked = isHovered && ImGui::IsMouseReleased(ImGuiMouseButton_Left);
        bool isDoubleClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
        bool isRightClicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);

        if (isClicked || isDoubleClicked)
        {
            searchSelectedPath = entry.path();
            Asset* asset = engine->assetDatabase->LoadAsset(
                std::filesystem::relative(entry.path(), engine->assetDatabase->GetAssetDirectory())
            );
            if (asset)
            {
                EditorState::SelectObject(asset);
            }
        }

        ImGui::SetCursorPos(cursorPos);

        if (isHovered && !isSelected)
        {
            ImVec2 bgMin = ImGui::GetCursorScreenPos();
            ImVec2 bgMax = {bgMin.x + buttonSize.x, bgMin.y + buttonSize.y};
            ImU32 bgColor = IM_COL32(70, 70, 70, 100);
            ImGui::GetWindowDrawList()->AddRectFilled(bgMin, bgMax, bgColor, 4.0f);
        }

        ImGui::Text("%s ", FileIcons::GetIcon(entry.path().extension()).c_str());
        ImGui::SameLine();

        if (isSelected)
        {
            ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), "%s", filename.c_str());
        }
        else
        {
            ImGui::Text("%s", filename.c_str());
        }

        ImGui::SameLine();
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled), "  %s", relativePath.parent_path().string().c_str());

        if (isRightClicked)
        {
            ImGui::OpenPopup("SearchResultContextMenu");
        }

        if (ImGui::BeginPopup("SearchResultContextMenu"))
        {
            AssetPath assetPath(entry.path());
            if (AssetDatabase::Singleton()->CanImport(assetPath) && ImGui::MenuItem("Reimport"))
            {
                RequestReimport({assetPath});
            }
            if (ImGui::MenuItem("Delete"))
            {
                RequestDelete({assetPath});
            }
            if (ImGui::MenuItem("Rename"))
            {
                ActivateFileNameField(entry.path());
            }
            ImGui::EndPopup();
        }

        std::filesystem::path filePath = entry.path().string();
        filePath = AssetPath(filePath);
        if (EditorGUI::DragDropSource(
                filePath,
                [filePath](Object*& obj)
                { obj = AssetDatabase::Singleton()->LoadAsset(filePath); }
            ))
        {
        }

        ImGui::PopID();
    }

    ImGui::EndChild();
}

Gfx::Image* AssetIcon::GetImage()
{
    // Return nullptr for now, let the calling code handle it
    return nullptr;
}

void AssetBrowser::ShowChangeFileNameField()
{
    if (changeFileName)
    {
        ImGui::OpenPopup("Change File Name");
        changeFileName = false;
        auto filename = changeFileNameTarget.ToFilesystemPath().filename().stem();
        auto ext = changeFileNameTarget.ToFilesystemPath().filename().extension();
        fileNameCache = filename.string();
        fileNameExtCache = ext;
    }
    if (ImGui::BeginPopupModal("Change File Name"))
    {
        EditorGUI::InputText("File Name: ", fileNameCache);

        if (ImGui::Selectable("Confirm") || ImGui::IsKeyPressed(ImGuiKey_Enter))
        {
            auto dir = changeFileNameTarget.ToFilesystemPath().parent_path();
            auto finalPath = dir / fileNameCache;
            finalPath.replace_extension(fileNameExtCache);
            const AssetPath oldPath = changeFileNameTarget;
            const AssetPath renamedPath(finalPath);
            AssetDatabase::Singleton()->Rename(oldPath, renamedPath);

            for (AssetPath& selectedPath : selectedPaths)
            {
                if (selectedPath == oldPath)
                {
                    selectedPath = renamedPath;
                }
            }
            if (activeSelectedPath == oldPath)
            {
                activeSelectedPath = renamedPath;
            }
            if (!searchSelectedPath.empty() && AssetPath(searchSelectedPath) == oldPath)
            {
                searchSelectedPath = renamedPath.ToAbsolutePath();
            }
            changeFileNameTarget = renamedPath;
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::Selectable("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void AssetBrowser::ActivateFileNameField(const AssetPath& path)
{
    changeFileName = true;
    changeFileNameTarget = path;
}

} // namespace Editor
