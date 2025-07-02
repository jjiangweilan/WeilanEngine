#include "AssetBrowser.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Asset.hpp"
#include "Core/GameObject.hpp"
#include "EditorGUI.hpp"
#include "EditorState.hpp"
#include "FileIcons.hpp"
#include "GameEditor.hpp"
#include "ThirdParty/imgui/imgui.h"
#include "WeilanEngine.hpp"

namespace Editor
{

AssetBrowser::AssetBrowser(WeilanEngine* engine, GameEditor* gameEditor)
    : engine(engine), gameEditor(gameEditor), currentDragDropAssetFileDepth(0)
{
    currentDirectory = engine->GetProjectAssetPath();
}

void AssetBrowser::Show(bool& isOpen)
{
    if (isOpen)
    {
        std::filesystem::path fullAssetsPath = engine->GetProjectPath() / "Assets";

        ImGui::Begin("Assets", &isOpen);
        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Create Folder"))
            {
                AssetDatabase::Singleton()->CreateFolderAtPath(fullAssetsPath);
            }

            ImGui::EndPopup();
        }

        ShowInternalAssets();
        ImGui::Separator();

        switch (mode)
        {
            case Mode::Tree: ShowDir(fullAssetsPath, 0);
            case Mode::Icon: ShowDirUsingIcon(currentDirectory, 0);
            default: break;
        }

        ImGui::End();
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
                if (GUI::DragDropSource(
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
    bool changeFileName = false;
    static std::filesystem::path changeFileNameTarget;

    // Need access to GameEditor's endEvents and endPopup
    auto& endEvents = gameEditor->endEvents;
    auto& endPopup = gameEditor->endPopup;

    for (auto entry : std::filesystem::directory_iterator(path))
    {
        if (entry.is_directory())
        {
            const std::filesystem::path& path = entry.path();
            auto relative = AssetDatabase::Singleton()->AbsolutePathToAssetPath(path);
            bool treeOpen = ImGui::TreeNodeEx(path.filename().string().c_str());

            if (GUI::DragDropSource(relative))
            {
                currentDragDropAssetFileDepth = depth;
            }

            std::filesystem::path pathStr;
            if (GUI::DragDropTarget(pathStr))
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
            GUI::DragDropTarget(typeid(GameObject), gameObject);

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
                                    AssetDatabase::Singleton()->AbsolutePathToAssetPath(entry.path())
                                );
                            }
                        );
                    }
                    else
                    {
                        endEvents.Register(
                            [entry]()
                            {
                                AssetDatabase::Singleton()->Remove(
                                    AssetDatabase::Singleton()->AbsolutePathToAssetPath(entry.path())
                                );
                            }
                        );
                    }
                }

                if (ImGui::MenuItem("Change File Name"))
                {
                    changeFileName = true;
                    changeFileNameTarget =
                        std::filesystem::relative(entry.path(), engine->assetDatabase->GetAssetDirectory());
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
        std::filesystem::path pathStr;
        if (GUI::DragDropTarget(pathStr, {currentCursor, contextRegionMax}))
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
            std::string pathStr = entry.path().filename().string();
            auto treeTitle = fmt::format("{} {}", FileIcons::GetIcon(entry.path().extension()), pathStr);
            bool open = ImGui::TreeNodeEx(treeTitle.c_str(), ImGuiTreeNodeFlags_Leaf);
            if (ImGui::BeginPopupContextItem("asset window context menu"))
            {
                if (ImGui::MenuItem("Change File Name"))
                {
                    changeFileName = true;
                    changeFileNameTarget =
                        std::filesystem::relative(entry.path(), engine->assetDatabase->GetAssetDirectory());
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
                path = AssetDatabase::Singleton()->AbsolutePathToAssetPath(path);
                GUI::DragDropSource(path, [path](Object*& obj) { obj = AssetDatabase::Singleton()->LoadAsset(path); });

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

    static char fn[1024];
    if (changeFileName)
    {
        ImGui::OpenPopup("Change File Name");
        auto filename = changeFileNameTarget.filename();
        strcpy(fn, filename.string().c_str());
    }
    if (ImGui::BeginPopupModal("Change File Name"))
    {
        ImGui::InputText("File Name: ", fn, 1024);

        if (ImGui::Selectable("Confirm"))
        {
            auto dir = changeFileNameTarget.parent_path();
            auto finalPath = dir / fn;
            AssetDatabase::Singleton()->Rename(changeFileNameTarget, finalPath);
        }
        if (ImGui::Selectable("Chancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void AssetBrowser::ShowDirUsingIcon(const std::filesystem::path& path, int depth)
{
    std::filesystem::path clickedPath = "";
    auto OnIconClicked = [&clickedPath]() {};
    auto OnIconRightClicked = []() {};

    for (const auto& entry : std::filesystem::directory_iterator(path))
    {
        AssetIcon* assetIcon = GetEditorAssetIcon(entry);
        ShowAssetIconWithName(path.filename(), assetIcon, GetIconSize(), OnIconClicked, OnIconRightClicked);
    }

    if (!clickedPath.empty())
    {
        ChangeCurrentDirectory();
    }
}

void AssetBrowser::ShowAssetIconWithName(
    const std::filesystem::path& name,
    AssetIcon* icon,
    int2 iconSize,
    std::function<void()> onClick,
    std::function<void()> onRightClick
)
{
    auto image = icon->GetImage();
}

Gfx::Image* AssetIcon::GetImage()
{
    return nullptr;
}

} // namespace Editor
