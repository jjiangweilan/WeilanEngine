#include "Editor/EditorConfig.hpp"
#include "Editor/EditorGUI.hpp"
#include "Editor/EditorState.hpp"
#include "Editor/GameEditor.hpp"
#include "Engine/Runtime/Object/GameObject/Prefab.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetDatabase.hpp"
#include "Engine/ThirdParty/imgui/imgui.h"

namespace Editor
{

static bool IsAncestorOf(GameObject* ancestor, GameObject* child)
{
    GameObject* parent = child->GetParent();
    while (parent != ancestor && parent != nullptr)
    {
        parent = parent->GetParent();
    }

    return parent == ancestor;
}

void GameEditor::ShowSceneTree(Scene& scene)
{
    ImGui::Begin("Hierarchy", nullptr, ImGuiWindowFlags_MenuBar);

    // Menu Bar
    ImGui::BeginMenuBar();
    if (ImGui::BeginMenu("Objects"))
    {
        if (ImGui::MenuItem("Create Object"))
        {
            scene.CreateGameObject();
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
    std::filesystem::path filePath;
    // drop a external object
    if (EditorGUI::DragDropTarget(filePath, {windowPos, windowMax}))
    {
        Object* asset = AssetDatabase::Singleton()->LoadAsset(filePath);
        if (Model* model = dynamic_cast<Model*>(asset))
        {
            auto gos = model->CreateGameObject();
            for (auto& go : gos)
                go->SetWantsToBeEnabled();
            scene.AddGameObjects(std::move(gos));
        }

        if (Prefab* prefab = dynamic_cast<Prefab*>(asset))
        {
            scene.AddGameObject(prefab->Instantiate());
        }
    }

    Object* moveToRoot = nullptr;
    if (EditorGUI::DragDropTarget(typeid(GameObject), moveToRoot, {windowPos, windowMax}))
    {
        GameObject* casted = static_cast<GameObject*>(moveToRoot);
        casted->SetParent(nullptr, true);
    }

    static GameObject* currentSelected = nullptr;
    bool autoExpand = false;
    GameObject* selected = dynamic_cast<GameObject*>(EditorState::GetMainSelectedObject());
    if (currentSelected != selected)
        autoExpand = true;
    currentSelected = selected;
    size_t imguiTreeId = 0;
    auto selects = EditorState::GetSelectedObjects();
    for (auto root : scene.GetRootObjects())
    {
        SceneTree(root, ++imguiTreeId, currentSelected, selects, autoExpand);
    }

    bool isSceneTreeWindowHovered = ImGui::IsWindowHovered();
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
            std::unique_ptr<Prefab> prefab = std::make_unique<Prefab>(sceneTreeContextObject);

            auto prefabName = prefab->GetGameObject()->GetName();
            AssetDatabase::Singleton()->SaveAsset(std::move(prefab), prefabName);
        }

        if (ImGui::Button("Create GameOject"))
        {
            auto go = scene.CreateGameObject();
            go->SetParent(sceneTreeContextObject);
        }

        if (ImGui::Button("Split Mesh Renderer"))
        {
            auto selected = dynamic_cast<GameObject*>(EditorState::GetMainSelectedObject());
            if (selected)
            {
                auto meshRenderer = selected->GetComponent<MeshRenderer>();
                auto meshes = meshRenderer->GetMeshes();
                auto materials = meshRenderer->GetMaterials();

                for (int i = 0; i < meshes.size() && i < materials.size(); ++i)
                {
                    auto mesh = meshes[i];
                    auto material = materials[i];

                    auto child = scene.CreateGameObject();
                    child->SetParent(selected, false);

                    auto m = child->AddComponent<MeshRenderer>();
                    m->SetMesh(mesh);
                    m->SetMaterial(material);
                }

                selected->RemoveComponent(meshRenderer);
            }
        }

        if (ImGui::Button("Delete"))
        {
            auto selects = EditorState::GetSelectedObjects();
            if (std::find_if(
                    selects.begin(),
                    selects.end(),
                    [this](ObjPtr<Object>& o)
                    { return o.Get() == sceneTreeContextObject; }
                ) != selects.end())
            {
                for (auto& s : selects)
                {
                    GameObject* ptr = static_cast<GameObject*>(s.Get());
                    if (ptr)
                        SceneManager::GetActiveScene()->DestroyGameObject(ptr);
                }
                ImGui::CloseCurrentPopup();
                sceneTreeContextObject = nullptr;
            }
            else
            {
                SceneManager::GetActiveScene()->DestroyGameObject(sceneTreeContextObject);
                ImGui::CloseCurrentPopup();
                sceneTreeContextObject = nullptr;
            }
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
                scene.CreateGameObject();
            }
            else if (ImGui::MenuItem("Cube"))
            {
                AddPrimitiveAssetToScene(scene, "_engine_internal/Models/Cube.fbx");
            }
            else if (ImGui::MenuItem("Sphere"))
            {
                AddPrimitiveAssetToScene(scene, "_engine_internal/Models/Sphere.fbx");
            }
            else if (ImGui::MenuItem("Plane"))
            {
                AddPrimitiveAssetToScene(scene, "_engine_internal/Models/Plane.fbx");
            }
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }
}

void GameEditor::SceneTree(
    GameObject* go, int imguiID, GameObject* currentSelected, std::vector<ObjPtr<Object>>& selects, bool autoExpand
)
{
    ImGuiTreeNodeFlags nodeFlags =
        ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;

    auto selectsIter = std::find_if(selects.begin(), selects.end(), [go](ObjPtr<Object>& o)
                                    { return o.Get() == go; });
    if (selectsIter != selects.end())
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
    bool treeOpen = ImGui::TreeNodeEx(fmt::format("{}##{}", go->GetName(), imguiID).c_str(), nodeFlags);
    if (hasPrefab)
        ImGui::PopStyleColor();

    if (ImGui::IsItemHovered())
    {
        // select game object
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            bool deselect = ImGui::IsKeyDown(ImGuiKey_LeftAlt);

            if (deselect)
            {
                EditorState::DeselectObject(go);
            }
            else
            {
                bool multiSelect = ImGui::IsKeyDown(ImGuiKey_LeftShift);
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

    EditorGUI::DragDropSource(go->GetName().c_str(), go);

    Object* dropGO;
    if (EditorGUI::DragDropTarget(typeid(GameObject), dropGO))
    {
        endEvents.Register([go, dropGO]()
                           {
            GameObject* casted = static_cast<GameObject*>(dropGO);
            casted->SetParent(go); });
    }

    if (EditorGUI::DragDropTarget(dropGO))
    {
        Component* asComponent = dynamic_cast<Component*>(dropGO);
        if (asComponent)
        {
            go->MoveInComponent(asComponent);
        }
    }

    if (treeOpen)
    {
        for (auto child : go->GetChildren())
        {
            SceneTree(child, ++imguiID, currentSelected, selects, autoExpand);
        }
        ImGui::TreePop();
    }
}
} // namespace Editor
