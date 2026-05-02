#include "Editor/AssetBrowser.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetDatabase.hpp"
#include "Engine/Core/Asset.hpp"
#include "Engine/Runtime/Object/GameObject/GameObject.hpp"
#include "Editor/EditorGUI.hpp"
#include "Editor/EditorState.hpp"
#include "Editor/FileIcons.hpp"
#include "Editor/GameEditor.hpp"
#include "Engine/ThirdParty/imgui/imgui.h"
#include "Engine/WeilanEngine.hpp"
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
} // namespace

AssetBrowser::AssetBrowser(WeilanEngine* engine, GameEditor* gameEditor)
    : engine(engine), gameEditor(gameEditor), currentDragDropAssetFileDepth(0)
{
    currentDirectory = engine->GetProjectAssetPath();
}

void AssetBrowser::Show(bool& isOpen)
{
    if (isOpen)
    {
        ENGINE_BEGIN_PROFILE("AssetBrowser - Show")

        std::filesystem::path fullAssetsPath = engine->GetProjectPath() / "Assets";

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

        ShowIconSizeSlider();

        // Determine mode based on icon size slider
        Mode effectiveMode = (iconSizeSlider <= TREE_MODE_THRESHOLD) ? Mode::Tree : Mode::Icon;

        switch (effectiveMode)
        {
            case Mode::Tree: ShowDir(fullAssetsPath, 0); break;
            case Mode::Icon: ShowDirUsingIcon(currentDirectory, 0); break;
            default: break;
        }

        // Show icon size slider in the lower right corner

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

    // Need access to GameEditor's endEvents and endPopup
    auto& endEvents = gameEditor->endEvents;
    auto& endPopup = gameEditor->endPopup;

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

                if (ImGui::MenuItem("Delete Folder"))
                {
                    if (!std::filesystem::is_empty(entry.path()))
                    {
                        endPopup.Show(
                            "Folder is not empty, delete all?",
                            [entry]()
                            {
                                AssetDatabase::Singleton()->Remove(
                                    AssetPath(entry.path())
                                );
                            }
                        );
                    }
                    else
                    {
                        endEvents.Register(
                            [this, entry]()
                            {
                                AssetDatabase::Singleton()->Remove(
                                    AssetPath(entry.path())
                                );
                            }
                        );
                    }
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
                    endEvents.Register(
                        [assetPath]()
                        {
                            AssetDatabase::Singleton()->Reimport(assetPath);
                        }
                    );
                }

                if (ImGui::MenuItem("Change File Name"))
                {
                    ActivateFileNameField(entry.path());
                }

                if (ImGui::MenuItem("Delete"))
                {
                    endEvents.Register(
                        [entry]()
                        {
                            AssetDatabase::Singleton()->Remove(
                                std::filesystem::relative(entry.path(), AssetDatabase::Singleton()->GetAssetDirectory())
                            );
                        }
                    );
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
    const float labelHeight = 40.0f;       // Height for the text label below icon
    const float totalItemWidth = iconSize + iconPadding * 2;
    const float totalItemHeight = iconSize + labelHeight + iconPadding * 2;
    bool openCreateMenuPopup = false;

    // Calculate how many icons fit horizontally
    ImVec2 contentRegion = ImGui::GetContentRegionAvail();
    int itemsPerRow = std::max(1, (int)(contentRegion.x / totalItemWidth));

    // Access to GameEditor's endEvents and endPopup
    auto& endEvents = gameEditor->endEvents;
    auto& endPopup = gameEditor->endPopup;

    // Navigation breadcrumb
    if (ImGui::Button("Up") && path != engine->GetProjectAssetPath())
    {
        currentDirectory = path.parent_path();
    }
    ImGui::SameLine();
    ImGui::Text("Current: %s", std::filesystem::relative(path, engine->GetProjectAssetPath()).string().c_str());

    ImGui::Separator();

    // Begin child region for scrolling
    if (ImGui::BeginChild("IconGrid", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar))
    {
        ImVec2 directoryCursorStart = ImGui::GetCursorPos();
        ImVec2 browserRegionMax = ImGui::GetWindowContentRegionMax();
        ImVec2 clickRegionSize = browserRegionMax - directoryCursorStart;

        ImGui::SetNextItemAllowOverlap();
        ImGui::InvisibleButton("right-click context menu", clickRegionSize);

        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
        {
            openCreateMenuPopup = true;
        }

        ImGui::SetCursorPos(directoryCursorStart);

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
    // Access to GameEditor's endEvents and endPopup here since we can't pass them as parameters
    auto& endEvents = gameEditor->endEvents;
    auto& endPopup = gameEditor->endPopup;

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

    // Begin group for the entire icon + label
    ImGui::BeginGroup();

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

    // Draw icon background
    //  And create invisible button for interaction
    ImVec2 cursorPos = ImGui::GetCursorPos();
    ImVec2 iconMin = ImGui::GetCursorScreenPos();
    ImVec2 iconMax = ImVec2(iconMin.x + iconSize, iconMin.y + iconSize);

    bool isHovered = false;
    bool isClicked = false;
    bool isDoubleClicked = false;
    bool isRightClicked = false;
    bool isLastSelection = false;

    // Update last selected path
    isLastSelection = lastSelectedPath == AssetPath(entry.path());

    ImGui::InvisibleButton("##icon", ImVec2(iconSize, iconSize));
    isHovered = ImGui::IsItemHovered();
    isClicked = isHovered && ImGui::IsMouseReleased(ImGuiMouseButton_Left);
    isDoubleClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
    isRightClicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);

    // Draw icon background
    ImU32 bgColor = isHovered ? IM_COL32(140, 140, 140, 100) : IM_COL32(70, 70, 70, 100);
    if (isLastSelection)
    {
        bgColor = IM_COL32(140, 140, 140, 200);
    }
    ImGui::GetWindowDrawList()->AddRectFilled(iconMin, iconMax, bgColor, 4.0f);

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

    // Display asset path
    float textWidth = iconMax.x - iconMin.x;
    ImVec2 labelPos = ImVec2(cursorPos.x, cursorPos.y + iconSize + 4);
    ImGui::SetCursorPos(labelPos);
    ImGui::PushTextWrapPos(cursorPos.x + textWidth);
    ImGui::Text("%s", filename.c_str());
    ImGui::PopTextWrapPos();

    ImGui::EndGroup();

    // Handle interactions
    if (isDoubleClicked)
    {
        if (isDirectory)
        {
            // Navigate into directory
            currentDirectory = entry.path();
        }
        else
        {
            // Load and select asset
            Asset* asset = engine->assetDatabase->LoadAsset(
                std::filesystem::relative(entry.path(), engine->assetDatabase->GetAssetDirectory())
            );
            if (asset)
            {
                EditorState::SelectObject(asset);
                UpdateLastSelection(entry.path());
            }
        }
    }
    else if (isClicked)
    {
        // Single click selection
        if (!isDirectory)
        {
            Asset* asset = engine->assetDatabase->LoadAsset(
                std::filesystem::relative(entry.path(), engine->assetDatabase->GetAssetDirectory())
            );
            if (asset)
            {
                EditorState::SelectObject(asset);
                UpdateLastSelection(entry.path());
            }
        }
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
        ImGui::OpenPopup("ItemContextMenu");
        UpdateLastSelection(entry.path());
    }

    if (ImGui::BeginPopup("ItemContextMenu"))
    {
        if (isDirectory)
        {
            if (ImGui::MenuItem("Create Folder"))
            {
                AssetDatabase::Singleton()->CreateFolderAtPath(entry.path());
            }

            if (ImGui::MenuItem("Delete Folder"))
            {
                if (!std::filesystem::is_empty(entry.path()))
                {
                    endPopup.Show(
                        "Folder is not empty, delete all?",
                        [entry]()
                        {
                            AssetDatabase::Singleton()->Remove(
                                AssetPath(entry.path())
                            );
                        }
                    );
                }
                else
                {
                    endEvents.Register(
                        [this, entry]()
                        {
                            AssetDatabase::Singleton()->Remove(
                                AssetPath(entry.path())
                            );
                        }
                    );
                }
            }

            if (ImGui::MenuItem("Rename"))
            {
                ActivateFileNameField(entry.path());
            }
        }
        else
        {
            AssetPath assetPath(entry.path());
            if (AssetDatabase::Singleton()->CanImport(assetPath) && ImGui::MenuItem("Reimport"))
            {
                endEvents.Register(
                    [assetPath]()
                    {
                        AssetDatabase::Singleton()->Reimport(assetPath);
                    }
                );
            }

            if (ImGui::MenuItem("Delete"))
            {
                endEvents.Register(
                    [entry]()
                    {
                        AssetDatabase::Singleton()->Remove(
                            std::filesystem::relative(entry.path(), AssetDatabase::Singleton()->GetAssetDirectory())
                        );
                    }
                );
            }

            if (ImGui::MenuItem("Rename"))
            {
                ActivateFileNameField(entry.path());
            }
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
        mode = (iconSizeSlider <= TREE_MODE_THRESHOLD) ? Mode::Tree : Mode::Icon;
    }
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
        strcpy(fileNameCache, filename.string().c_str());
        fileNameExtCache = ext;
    }
    if (ImGui::BeginPopupModal("Change File Name"))
    {
        ImGui::InputText("File Name: ", fileNameCache, 1024);

        if (ImGui::Selectable("Confirm") || ImGui::IsKeyPressed(ImGuiKey_Enter))
        {
            auto dir = changeFileNameTarget.ToFilesystemPath().parent_path();
            auto finalPath = dir / fileNameCache;
            finalPath.replace_extension(fileNameExtCache);
            AssetDatabase::Singleton()->Rename(changeFileNameTarget, AssetPath(finalPath));
        }
        if (ImGui::Selectable("Chancel") || ImGui::IsKeyPressed(ImGuiKey_Backspace))
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
