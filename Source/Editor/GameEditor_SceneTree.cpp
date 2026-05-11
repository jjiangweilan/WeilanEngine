#include "Editor/EditorConfig.hpp"
#include "Editor/EditorGUI.hpp"
#include "Editor/EditorState.hpp"
#include "Editor/GameEditor.hpp"
#include "Engine/Runtime/Object/GameObject/Prefab.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetDatabase.hpp"
#include "Engine/ThirdParty/imgui/imgui.h"
#include "Engine/Runtime/Object/Mesh/Model.hpp"

namespace Editor
{

static void BuildSceneTreeFlatList(Scene& scene, std::vector<GameObject*>& flatList)
{
    auto add_to_list = [&](GameObject* go, auto& self) -> void {
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
                { return std::vector<GameObject*>{scene.CreateGameObject()}; }
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

    Object* moveToRoot = nullptr;
    if (EditorGUI::DragDropTarget(typeid(GameObject), moveToRoot, {windowPos, windowMax}))
    {
        std::vector<GameObject*> draggedGameObjects = ResolveDraggedGameObjects(static_cast<GameObject*>(moveToRoot));
        endEvents.Register([draggedGameObjects]()
                           {
            EditorState::GetUndoManager().CaptureGameObjectHierarchyChange(
                "Reparent GameObject",
                GetReparentUndoTargets(draggedGameObjects),
                [draggedGameObjects]()
                {
                    for (GameObject* gameObject : draggedGameObjects) {
                        gameObject->SetParent(nullptr, true);
                    }
                }
            ); });
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

    for (auto root : scene.GetRootObjects())
    {
        SceneTree(root, scene, lastSelectedGameObject.Get(), selects, autoExpand, lazyFlatList, flatList);
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
        if (ImGui::Button("Create Prefab"))
        {
            auto& undoManager = EditorState::GetUndoManager();
            undoManager.BeginTransaction("Create Prefab");
            std::unique_ptr<Prefab> prefab = std::make_unique<Prefab>(sceneTreeContextObject);

            auto prefabName = prefab->GetGameObject()->GetName();
            Asset* savedPrefab = AssetDatabase::Singleton()->SaveAsset(std::move(prefab), prefabName);
            undoManager.TrackCreatedAsset(savedPrefab);
            undoManager.EndTransaction();
        }

        if (ImGui::Button("Create GameOject"))
        {
            EditorState::GetUndoManager().CaptureGameObjectCreation(
                "Create GameObject",
                [this, &scene]()
                {
                    auto go = scene.CreateGameObject();
                    go->SetParent(sceneTreeContextObject);
                    return std::vector<GameObject*>{go};
                }
            );
        }

        if (ImGui::Button("Split Mesh Renderer"))
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
        }

        if (ImGui::Button("Delete"))
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
            ImGui::CloseCurrentPopup();
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
                    { return std::vector<GameObject*>{scene.CreateGameObject()}; }
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
    std::vector<GameObject*>& flatListCache,
    std::vector<GameObject*>*& flatList
)
{
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

    if (go->GetChildren().empty())
        nodeFlags |= ImGuiTreeNodeFlags_Leaf;

    bool hasPrefab = go->HasPrefab();

    auto& editorConfig = EditorConfig::GetInstance();
    if (hasPrefab)
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(editorConfig.GetSceneTreeGameObjectColor()));
    bool treeOpen = ImGui::TreeNodeEx(fmt::format("{}##{:p}", go->GetName(), (void*)go).c_str(), nodeFlags);

    bool itemHovered = ImGui::IsItemHovered();
    if (itemHovered)
    {
        sceneViewHightedGameObjectCandidate = go;
    }

    if (hasPrefab)
        ImGui::PopStyleColor();

    EditorGUI::DragDropSource(go->GetName().c_str(), go);

    bool acceptedGameObjectDrop = false;
    Object* dropGO;
    if (EditorGUI::DragDropTarget(typeid(GameObject), dropGO))
    {
        acceptedGameObjectDrop = true;
        std::vector<GameObject*> draggedGameObjects = ResolveDraggedGameObjects(static_cast<GameObject*>(dropGO));
        endEvents.Register([go, draggedGameObjects]()
                           {
            EditorState::GetUndoManager().CaptureGameObjectHierarchyChange(
                "Reparent GameObject",
                GetReparentUndoTargets(draggedGameObjects),
                [go, draggedGameObjects]()
                {
                    for (GameObject* gameObject : draggedGameObjects) {
                        if (gameObject != go && !IsAncestorOf(gameObject, go)) {
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
            SceneTree(child, scene, currentSelected, selects, autoExpand, flatListCache, flatList);
        }
        ImGui::TreePop();
    }
}
} // namespace Editor
