#include "Editor/EditorConfig.hpp"
#include "Editor/EditorGUI.hpp"
#include "Editor/EditorState.hpp"
#include "Editor/GameEditor.hpp"
#include "Editor/ImGuiStyleSheet.hpp"
#include "Engine/Runtime/Object/GameObject/Prefab.hpp"
#include "Engine/Runtime/Object/Mesh/Model.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetDatabase.hpp"
#include "Engine/ThirdParty/imgui/imgui.h"
#include <algorithm>
#include <cctype>
#include <string_view>

namespace Editor
{

enum SceneTreeSearchMode
{
    SceneTreeSearchMode_GameObject = 0,
    SceneTreeSearchMode_Component = 1
};

static bool ContainsCaseInsensitive(std::string_view text, std::string_view query)
{
    if (query.empty())
        return true;

    auto lower = [](char c)
    { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); };

    auto iter = std::search(
        text.begin(),
        text.end(),
        query.begin(),
        query.end(),
        [lower](char a, char b)
        { return lower(a) == lower(b); }
    );
    return iter != text.end();
}

static bool GameObjectMatchesSceneTreeSearch(GameObject* go, int searchMode, std::string_view query)
{
    if (go == nullptr)
        return false;

    if (searchMode == SceneTreeSearchMode_GameObject)
        return ContainsCaseInsensitive(go->GetName(), query);

    for (Component* component : go->GetComponents())
    {
        if (component != nullptr && ContainsCaseInsensitive(component->GetName(), query))
            return true;
    }
    return false;
}

static bool SceneTreeHasSearchMatch(GameObject* go, int searchMode, std::string_view query)
{
    if (GameObjectMatchesSceneTreeSearch(go, searchMode, query))
        return true;

    for (auto child : go->GetChildren())
    {
        if (SceneTreeHasSearchMatch(child, searchMode, query))
            return true;
    }
    return false;
}

static void BuildSceneTreeFlatList(Scene& scene, std::vector<GameObject*>& flatList)
{
    auto add_to_list = [&](GameObject* go, auto& self) -> void
    {
        flatList.push_back(go);
        for (auto child : go->GetChildren())
        {
            self(child, self);
        }
    };

    for (auto root : scene.GetRootObjects())
    {
        add_to_list(root, add_to_list);
    }
}

static bool IsAncestorOf(GameObject* ancestor, GameObject* child)
{
    if (ancestor == nullptr || child == nullptr)
        return false;

    GameObject* parent = child->GetParent();
    while (parent != ancestor && parent != nullptr)
    {
        parent = parent->GetParent();
    }

    return parent == ancestor;
}

static std::vector<GameObject*> ResolveDraggedGameObjects(GameObject* draggedObject)
{
    auto selects = EditorState::GetSelectedObjects();
    bool draggedIsSelected = std::find_if(selects.begin(), selects.end(), [draggedObject](const ObjPtr<Object>& selected)
                                          { return selected.Get() == draggedObject; }) != selects.end();

    if (!draggedIsSelected)
    {
        return {draggedObject};
    }

    std::vector<GameObject*> draggedGameObjects;
    draggedGameObjects.reserve(selects.size());

    for (auto& selected : selects)
    {
        GameObject* selectedGameObject = dynamic_cast<GameObject*>(selected.Get());
        if (selectedGameObject == nullptr)
        {
            continue;
        }

        bool hasSelectedAncestor = false;
        for (auto& potentialAncestor : selects)
        {
            GameObject* ancestorGameObject = dynamic_cast<GameObject*>(potentialAncestor.Get());
            if (ancestorGameObject == nullptr || ancestorGameObject == selectedGameObject)
            {
                continue;
            }

            if (IsAncestorOf(ancestorGameObject, selectedGameObject))
            {
                hasSelectedAncestor = true;
                break;
            }
        }

        if (!hasSelectedAncestor)
        {
            draggedGameObjects.push_back(selectedGameObject);
        }
    }

    if (draggedGameObjects.empty())
    {
        draggedGameObjects.push_back(draggedObject);
    }

    return draggedGameObjects;
}

static std::vector<GameObject*> GetReparentUndoTargets(const std::vector<GameObject*>& movedObjects)
{
    std::vector<GameObject*> targets;
    auto addUnique = [&targets](GameObject* gameObject)
    {
        if (gameObject != nullptr && std::find(targets.begin(), targets.end(), gameObject) == targets.end())
            targets.push_back(gameObject);
    };

    for (GameObject* gameObject : movedObjects)
        addUnique(gameObject);
    return targets;
}

static int GetSiblingIndex(Scene& scene, GameObject* gameObject)
{
    if (gameObject == nullptr)
        return -1;

    const auto& siblings = gameObject->GetParent() ? gameObject->GetParent()->GetChildren() : scene.GetRootObjects();
    auto iter = std::find_if(siblings.begin(), siblings.end(), [gameObject](const ObjPtr<GameObject>& sibling)
                             { return sibling.Get() == gameObject; });
    return iter == siblings.end() ? -1 : static_cast<int>(std::distance(siblings.begin(), iter));
}

static void ReorderGameObjects(Scene& scene, const std::vector<GameObject*>& draggedGameObjects, GameObject* parent, int siblingIndex)
{
    int currentIndex = siblingIndex;
    for (GameObject* gameObject : draggedGameObjects)
    {
        if (gameObject == nullptr || gameObject == parent || IsAncestorOf(gameObject, parent))
            continue;

        scene.MoveGameObjectToParentIndex(gameObject, parent, currentIndex);
        ++currentIndex;
    }
}

void GameEditor::ShowSceneTreeSearchBar()
{
    const bool componentMode = sceneTreeSearchMode == SceneTreeSearchMode_Component;
    const char* modeLabel = componentMode ? "C##SceneTreeSearchMode" : "G##SceneTreeSearchMode";
    const char* hint = componentMode ? "Search Components..." : "Search GameObjects...";

    {
        auto imguiStyleSheet = ImGuiStyleSheet::AccentToggleButton(componentMode);
        ScopedImGuiButtonStyle scopedStyle(imguiStyleSheet);
        if (ImGui::Button(modeLabel, ImGuiStyleSheet::FrameButtonSize(imguiStyleSheet)))
        {
            sceneTreeSearchMode = componentMode ? SceneTreeSearchMode_GameObject : SceneTreeSearchMode_Component;
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(componentMode ? "C: find by component type" : "G: find by GameObject name");
        }
    }

    ImGui::SameLine(0.0f, 6.0f);

    float clearButtonWidth = sceneTreeSearchQuery.empty() ? 0.0f : ImGui::GetFrameHeight();
    float inputWidth = ImGui::GetContentRegionAvail().x - clearButtonWidth;
    if (clearButtonWidth > 0.0f)
        inputWidth -= ImGui::GetStyle().ItemSpacing.x;
    inputWidth = std::max(80.0f, inputWidth);

    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.10f, 0.11f, 0.14f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.14f, 0.16f, 0.21f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.16f, 0.18f, 0.24f, 1.0f));
    ImGui::PushItemWidth(inputWidth);
    EditorGUI::InputText("##SceneTreeSearch", sceneTreeSearchQuery, hint);
    ImGui::PopItemWidth();
    ImGui::PopStyleColor(3);

    if (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Escape) && !sceneTreeSearchQuery.empty())
        sceneTreeSearchQuery.clear();

    if (!sceneTreeSearchQuery.empty())
    {
        ImGui::SameLine();
        if (ImGui::Button("x##SceneTreeSearchClear", ImVec2(clearButtonWidth, 0.0f)))
            sceneTreeSearchQuery.clear();
    }

    ImGui::Spacing();
}

void GameEditor::ShowSceneTree(Scene& scene)
{
    ENGINE_BEGIN_PROFILE("ShowSceneTree");

    ImGui::Begin("Hierarchy", nullptr, ImGuiWindowFlags_MenuBar);

    // Menu Bar
    ImGui::BeginMenuBar();
    if (ImGui::BeginMenu("Objects"))
    {
        if (ImGui::MenuItem("Create Object"))
        {
            EditorState::GetUndoManager().CaptureGameObjectCreation(
                "Create GameObject",
                [&scene]()
                {
                    auto gameObject = scene.CreateGameObject();
                    EditorState::SelectObject(gameObject);
                    return std::vector<GameObject*>{gameObject};
                }
            );
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Objects"))
    {
        if (ImGui::MenuItem("Ok"))
            spdlog::info("ok");
        ImGui::EndMenu();
    }
    ImGui::EndMenuBar();

    ShowSceneTreeSearchBar();

    auto windowPos = ImGui::GetWindowPos();
    auto windowMax = windowPos + ImVec2{ImGui::GetWindowWidth(), ImGui::GetWindowHeight()};
    AssetPath filePath;
    // drop a external object
    if (EditorGUI::DragDropTarget(filePath, {windowPos, windowMax}))
    {
        Object* asset = AssetDatabase::Singleton()->LoadAsset(filePath);
        if (Model* model = dynamic_cast<Model*>(asset))
        {
            EditorState::GetUndoManager().CaptureGameObjectCreation(
                "Add Model To Scene",
                [&scene, model]()
                {
                    auto gos = model->CreateGameObject();
                    std::vector<GameObject*> createdGameObjects;
                    createdGameObjects.reserve(gos.size());
                    for (auto& go : gos)
                    {
                        go->SetWantsToBeEnabled();
                        createdGameObjects.push_back(go.get());
                    }
                    scene.AddGameObjects(std::move(gos));
                    return createdGameObjects;
                }
            );
        }

        if (Prefab* prefab = dynamic_cast<Prefab*>(asset))
        {
            EditorState::GetUndoManager().CaptureGameObjectCreation(
                "Spawn Prefab",
                [&scene, prefab]()
                {
                    ObjPtr<GameObject> gameObject = scene.SpawnPrefab(prefab);
                    return std::vector<GameObject*>{gameObject.Get()};
                }
            );
        }
    }

    bool autoExpand = false;
    auto mainSelected = EditorState::GetMainSelectedObject();
    GameObject* selectedGameObject = dynamic_cast<GameObject*>(mainSelected);

    if (mainSelected == nullptr)
    {
        lastSelectedGameObject = nullptr;
    }
    else if (selectedGameObject != nullptr)
    {
        if (lastSelectedGameObject.Get() != selectedGameObject)
            autoExpand = true;
        lastSelectedGameObject = selectedGameObject;
    }

    auto selects = EditorState::GetSelectedObjects();
    sceneViewHightedGameObjectCandidate = nullptr; // reselect highted GameObject

    std::vector<GameObject*> lazyFlatList;
    std::vector<GameObject*>* flatList = nullptr;
    bool searchActive = !sceneTreeSearchQuery.empty();

    for (auto root : scene.GetRootObjects())
    {
        SceneTree(
            root,
            scene,
            lastSelectedGameObject.Get(),
            selects,
            autoExpand,
            searchActive,
            sceneTreeSearchMode,
            sceneTreeSearchQuery,
            lazyFlatList,
            flatList
        );
    }

    ImVec2 rootDropMin = ImGui::GetCursorScreenPos();
    ImVec2 rootDropMax = ImVec2(windowMax.x, rootDropMin.y + ImGui::GetTextLineHeightWithSpacing());
    Object* rootEndDropGO = nullptr;
    if (!searchActive && EditorGUI::DragDropTarget(typeid(GameObject), rootEndDropGO, {rootDropMin, rootDropMax}))
    {
        std::vector<GameObject*> draggedGameObjects = ResolveDraggedGameObjects(static_cast<GameObject*>(rootEndDropGO));
        int targetIndex = static_cast<int>(scene.GetRootObjects().size());
        endEvents.Register([&scene, draggedGameObjects, targetIndex]()
                           { EditorState::GetUndoManager().CaptureGameObjectHierarchyChange(
                                 "Reorder GameObject",
                                 GetReparentUndoTargets(draggedGameObjects),
                                 [&scene, draggedGameObjects, targetIndex]()
                                 { ReorderGameObjects(scene, draggedGameObjects, nullptr, targetIndex); }
                             ); });
    }

    bool isSceneTreeWindowHovered = ImGui::IsWindowHovered();
    editorContext->HighlightGameObject(isSceneTreeWindowHovered ? sceneViewHightedGameObjectCandidate : nullptr);
    ImGui::End();

    // context menu of scene tree
    static const char* gameObjectContextMenu = "GameObject Context Menu";
    static const char* sceneTreeContextMenu = "Scene Tree Context Menu";
    if (beginSceneTreeContextPopup)
    {
        beginSceneTreeContextPopup = false;
        ImGui::OpenPopup(gameObjectContextMenu);
    }

    if (ImGui::BeginPopup(gameObjectContextMenu))
    {
        if (ImGui::MenuItem("Create Prefab"))
        {
            auto& undoManager = EditorState::GetUndoManager();
            undoManager.BeginTransaction("Create Prefab");
            std::unique_ptr<Prefab> prefab = std::make_unique<Prefab>(sceneTreeContextObject);

            auto prefabName = prefab->GetGameObject()->GetName();
            AssetPath prefabPath = assetBrowser->GetCurrentDirectory() / prefabName;
            Asset* savedPrefab = AssetDatabase::Singleton()->SaveAsset(std::move(prefab), prefabPath);
            undoManager.TrackCreatedAsset(savedPrefab);
            undoManager.EndTransaction();
            sceneTreeContextObject = nullptr;
        }

        if (ImGui::MenuItem("Create GameObject"))
        {
            EditorState::GetUndoManager().CaptureGameObjectCreation(
                "Create GameObject",
                [this, &scene]()
                {
                    auto go = scene.CreateGameObject();
                    go->SetParent(sceneTreeContextObject);
                    EditorState::SelectObject(go);
                    return std::vector<GameObject*>{go};
                }
            );
            sceneTreeContextObject = nullptr;
        }

        if (ImGui::MenuItem("Split Mesh Renderer"))
        {
            auto selected = dynamic_cast<GameObject*>(EditorState::GetMainSelectedObject());
            if (selected)
            {
                auto meshRenderer = selected->GetComponent<MeshRenderer>();
                auto meshes = meshRenderer->GetMeshes();
                auto materials = meshRenderer->GetMaterials();

                auto& undoManager = EditorState::GetUndoManager();
                undoManager.BeginTransaction("Split Mesh Renderer");
                undoManager.TrackGameObject(selected);

                std::vector<GameObject*> createdGameObjects;
                for (int i = 0; i < meshes.size() && i < materials.size(); ++i)
                {
                    auto mesh = meshes[i];
                    auto material = materials[i];

                    auto child = scene.CreateGameObject();
                    child->SetParent(selected, false);
                    createdGameObjects.push_back(child);

                    auto m = child->AddComponent<MeshRenderer>();
                    m->SetMesh(mesh);
                    m->SetMaterial(material);
                }

                selected->RemoveComponent(meshRenderer);

                for (GameObject* gameObject : createdGameObjects)
                    undoManager.TrackCreatedGameObject(gameObject, true);
                undoManager.EndTransaction();
            }
            sceneTreeContextObject = nullptr;
        }

        if (ImGui::MenuItem("Delete"))
        {
            std::vector<GameObject*> objectsToDelete = ResolveDraggedGameObjects(sceneTreeContextObject);
            EditorState::GetUndoManager().CaptureGameObjectDeletion(
                "Delete GameObject",
                objectsToDelete,
                [objectsToDelete]()
                {
                    for (GameObject* gameObject : objectsToDelete)
                    {
                        if (gameObject && gameObject->GetScene())
                            gameObject->GetScene()->DestroyGameObject(gameObject);
                    }
                }
            );
            sceneTreeContextObject = nullptr;
        }
        ImGui::EndPopup();
    }

    // scene tree context menu
    if (!ImGui::IsPopupOpen(gameObjectContextMenu) && ImGui::IsMouseReleased(ImGuiMouseButton_Right) &&
        isSceneTreeWindowHovered)
    {
        ImGui::OpenPopup(sceneTreeContextMenu);
    }
    if (ImGui::BeginPopup(sceneTreeContextMenu))
    {
        if (ImGui::BeginMenu("Create Objects"))
        {
            if (ImGui::MenuItem("New GameObject"))
            {
                EditorState::GetUndoManager().CaptureGameObjectCreation(
                    "Create GameObject",
                    [&scene]()
                    {
                        auto gameObject = scene.CreateGameObject();
                        EditorState::SelectObject(gameObject);
                        return std::vector<GameObject*>{gameObject};
                    }
                );
            }
            else if (ImGui::MenuItem("Cube"))
            {
                EditorState::GetUndoManager().CaptureGameObjectCreation(
                    "Create Cube",
                    [this, &scene]()
                    { return std::vector<GameObject*>{AddPrimitiveAssetToScene(scene, "_engine_internal/Models/Cube.fbx")}; }
                );
            }
            else if (ImGui::MenuItem("Sphere"))
            {
                EditorState::GetUndoManager().CaptureGameObjectCreation(
                    "Create Sphere",
                    [this, &scene]()
                    { return std::vector<GameObject*>{AddPrimitiveAssetToScene(scene, "_engine_internal/Models/Sphere.fbx")}; }
                );
            }
            else if (ImGui::MenuItem("Plane"))
            {
                EditorState::GetUndoManager().CaptureGameObjectCreation(
                    "Create Plane",
                    [this, &scene]()
                    { return std::vector<GameObject*>{AddPrimitiveAssetToScene(scene, "_engine_internal/Models/Plane.fbx")}; }
                );
            }
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }

    ENGINE_END_PROFILE;
}

void GameEditor::SceneTree(
    GameObject* go,
    Scene& scene,
    GameObject* currentSelected,
    std::vector<ObjPtr<Object>>& selects,
    bool autoExpand,
    bool searchActive,
    int searchMode,
    std::string_view searchQuery,
    std::vector<GameObject*>& flatListCache,
    std::vector<GameObject*>*& flatList
)
{
    if (searchActive && !SceneTreeHasSearchMatch(go, searchMode, searchQuery))
        return;

    ImGuiTreeNodeFlags nodeFlags =
        ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;

    auto selectsIter = std::find_if(selects.begin(), selects.end(), [go](ObjPtr<Object>& o)
                                    { return o.Get() == go; });
    if (selectsIter != selects.end() || go == currentSelected)
    {
        nodeFlags |= ImGuiTreeNodeFlags_Selected;
    }

    if (autoExpand && currentSelected != nullptr && IsAncestorOf(go, currentSelected))
        ImGui::SetNextItemOpen(true);

    if (searchActive)
        ImGui::SetNextItemOpen(true);

    if (go->GetChildren().empty())
        nodeFlags |= ImGuiTreeNodeFlags_Leaf;

    bool hasPrefab = go->HasPrefab();

    auto& editorConfig = EditorConfig::GetInstance();
    if (hasPrefab)
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(editorConfig.GetSceneTreeGameObjectColor()));
    bool treeOpen = ImGui::TreeNodeEx(fmt::format("{}##{:p}", go->GetName(), (void*)go).c_str(), nodeFlags);
    ImVec2 itemRectMin = ImGui::GetItemRectMin();
    ImVec2 itemRectMax = ImGui::GetItemRectMax();

    bool itemHovered = ImGui::IsItemHovered();
    if (itemHovered)
    {
        sceneViewHightedGameObjectCandidate = go;
    }

    if (hasPrefab)
        ImGui::PopStyleColor();

    EditorGUI::DragDropSource(go->GetName().c_str(), go);

    bool acceptedGameObjectDrop = false;
    Object* dropGO = nullptr;
    float reorderDropZoneHeight = std::max(2.0f, (itemRectMax.y - itemRectMin.y) * 0.25f);
    ImRect aboveDropRect(itemRectMin, ImVec2(itemRectMax.x, itemRectMin.y + reorderDropZoneHeight));
    ImRect belowDropRect(ImVec2(itemRectMin.x, itemRectMax.y - reorderDropZoneHeight), itemRectMax);

    auto registerReorderDrop = [&](int targetIndex)
    {
        std::vector<GameObject*> draggedGameObjects = ResolveDraggedGameObjects(static_cast<GameObject*>(dropGO));
        if (std::find(draggedGameObjects.begin(), draggedGameObjects.end(), go) != draggedGameObjects.end())
            return;

        GameObject* parent = go->GetParent();
        endEvents.Register([&scene, draggedGameObjects, parent, targetIndex]()
                           { EditorState::GetUndoManager().CaptureGameObjectHierarchyChange(
                                 "Reorder GameObject",
                                 GetReparentUndoTargets(draggedGameObjects),
                                 [&scene, draggedGameObjects, parent, targetIndex]()
                                 { ReorderGameObjects(scene, draggedGameObjects, parent, targetIndex); }
                             ); });
    };

    if (EditorGUI::DragDropTarget(typeid(GameObject), dropGO, aboveDropRect))
    {
        acceptedGameObjectDrop = true;
        int targetIndex = GetSiblingIndex(scene, go);
        if (targetIndex >= 0)
            registerReorderDrop(targetIndex);
    }

    if (!acceptedGameObjectDrop && EditorGUI::DragDropTarget(typeid(GameObject), dropGO, belowDropRect))
    {
        acceptedGameObjectDrop = true;
        int targetIndex = GetSiblingIndex(scene, go);
        if (targetIndex >= 0)
            registerReorderDrop(targetIndex + 1);
    }

    if (!acceptedGameObjectDrop && EditorGUI::DragDropTarget(typeid(GameObject), dropGO))
    {
        acceptedGameObjectDrop = true;
        std::vector<GameObject*> draggedGameObjects = ResolveDraggedGameObjects(static_cast<GameObject*>(dropGO));
        endEvents.Register([go, draggedGameObjects]()
                           { EditorState::GetUndoManager().CaptureGameObjectHierarchyChange(
                                 "Reparent GameObject",
                                 GetReparentUndoTargets(draggedGameObjects),
                                 [go, draggedGameObjects]()
                                 {
                                     for (GameObject* gameObject : draggedGameObjects)
                                     {
                                         if (gameObject != go && !IsAncestorOf(gameObject, go))
                                         {
                                             gameObject->SetParent(go);
                                         }
                                     }
                                 }
                             ); });
    }

    if (itemHovered)
    {
        // select game object
        if (!acceptedGameObjectDrop && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            bool deselect = ImGui::IsKeyDown(ImGuiKey_LeftAlt);
            bool multiSelect = ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl);
            bool shiftSelect = ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift);

            if (deselect)
            {
                EditorState::DeselectObject(go);
            }
            else if (shiftSelect && currentSelected != nullptr)
            {
                if (flatList == nullptr)
                {
                    BuildSceneTreeFlatList(scene, flatListCache);
                    flatList = &flatListCache;
                }

                auto& resolvedFlatList = *flatList;
                auto it1 = std::find(resolvedFlatList.begin(), resolvedFlatList.end(), currentSelected);
                auto it2 = std::find(resolvedFlatList.begin(), resolvedFlatList.end(), go);

                if (it1 != resolvedFlatList.end() && it2 != resolvedFlatList.end())
                {
                    int startIdx =
                        std::min(std::distance(resolvedFlatList.begin(), it1), std::distance(resolvedFlatList.begin(), it2));
                    int endIdx =
                        std::max(std::distance(resolvedFlatList.begin(), it1), std::distance(resolvedFlatList.begin(), it2));

                    if (!multiSelect)
                    {
                        EditorState::SelectObject(nullptr, false);
                    }

                    EditorState::SelectObject(currentSelected, true);
                    for (int i = startIdx; i <= endIdx; ++i)
                    {
                        if (resolvedFlatList[i] != currentSelected)
                            EditorState::SelectObject(resolvedFlatList[i], true);
                    }
                }
                else
                {
                    EditorState::SelectObject(go, multiSelect);
                }
            }
            else
            {
                EditorState::SelectObject(go, multiSelect);
            }
        }

        // open context tree
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Right))
        {
            sceneTreeContextObject = go;
            beginSceneTreeContextPopup = true;
        }
    }

    if (EditorGUI::DragDropTarget(dropGO))
    {
        Component* asComponent = dynamic_cast<Component*>(dropGO);
        if (asComponent)
        {
            EditorState::GetUndoManager().CaptureGameObjectChange(
                "Move Component",
                {go, asComponent->GetGameObject()},
                [go, asComponent]()
                { go->MoveInComponent(asComponent); }
            );
        }
    }

    if (treeOpen)
    {
        for (auto child : go->GetChildren())
        {
            SceneTree(child, scene, currentSelected, selects, autoExpand, searchActive, searchMode, searchQuery, flatListCache, flatList);
        }
        ImGui::TreePop();
    }
}
} // namespace Editor
