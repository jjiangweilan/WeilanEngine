#include "Editor/UndoManager.hpp"
#include "Editor/EditorState.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetDatabase.hpp"
#include "Engine/Runtime/System/SceneManager/SceneManager.hpp"
#include <algorithm>
#include <unordered_set>
#include <unordered_map>
#include <spdlog/spdlog.h>

namespace Editor
{
namespace
{
const std::string EmptyCommandName = "";

class TransactionCommand final : public UndoCommand
{
public:
    TransactionCommand(
        UndoManager& manager,
        std::string name,
        std::vector<UndoManager::UndoEntityRecord> records,
        std::vector<UUID> beforeSelection,
        std::vector<UUID> afterSelection
    )
        : manager(manager),
          name(std::move(name)),
          records(std::move(records)),
          beforeSelection(std::move(beforeSelection)),
          afterSelection(std::move(afterSelection))
    {}

    void Undo() override
    {
        std::vector<UndoManager::UndoEntityState> states;
        states.reserve(records.size());
        for (auto& record : records)
        {
            states.push_back(record.before);
        }
        manager.ApplyTransactionState(states);
        UndoManager::RestoreSelection(beforeSelection);
    }

    void Redo() override
    {
        std::vector<UndoManager::UndoEntityState> states;
        states.reserve(records.size());
        for (auto& record : records)
        {
            states.push_back(record.after);
        }
        manager.ApplyTransactionState(states);
        UndoManager::RestoreSelection(afterSelection);
    }

    const std::string& GetName() const override { return name; }

private:
    UndoManager& manager;
    std::string name;
    std::vector<UndoManager::UndoEntityRecord> records;
    std::vector<UUID> beforeSelection;
    std::vector<UUID> afterSelection;
};

bool HasStateChanged(const UndoManager::UndoEntityState& before, const UndoManager::UndoEntityState& after)
{
    return before.exists != after.exists || before.typeID != after.typeID || before.data != after.data ||
           before.assetPath != after.assetPath || before.scene != after.scene ||
           before.gameObjectMode != after.gameObjectMode || before.parentUUID != after.parentUUID ||
           before.siblingIndex != after.siblingIndex || before.isRoot != after.isRoot ||
           before.localPosition != after.localPosition || before.localRotation != after.localRotation ||
           before.localScale != after.localScale;
}

std::vector<UUID> GetSerializedChildUUIDs(const nlohmann::json& state)
{
    std::vector<UUID> childUUIDs;
    if (!state.is_object() || !state.contains("children") || !state["children"].is_array())
        return childUUIDs;

    for (const auto& child : state["children"])
    {
        if (child.is_string())
            childUUIDs.emplace_back(child.get<std::string>());
    }

    return childUUIDs;
}
} // namespace

void UndoManager::Execute(std::unique_ptr<UndoCommand>&& command)
{
    if (command == nullptr || applying)
        return;

    applying = true;
    command->Redo();
    applying = false;
    Push(std::move(command));
}

void UndoManager::Undo()
{
    if (!CanUndo() || applying)
        return;

    auto command = std::move(undoStack.back());
    undoStack.pop_back();

    applying = true;
    command->Undo();
    applying = false;

    redoStack.push_back(std::move(command));
}

void UndoManager::Redo()
{
    if (!CanRedo() || applying)
        return;

    auto command = std::move(redoStack.back());
    redoStack.pop_back();

    applying = true;
    command->Redo();
    applying = false;

    undoStack.push_back(std::move(command));
}

void UndoManager::Clear()
{
    undoStack.clear();
    redoStack.clear();
    activeTransaction = {};
}

const std::string& UndoManager::GetUndoName() const
{
    return CanUndo() ? undoStack.back()->GetName() : EmptyCommandName;
}

const std::string& UndoManager::GetRedoName() const
{
    return CanRedo() ? redoStack.back()->GetName() : EmptyCommandName;
}

void UndoManager::BeginTransaction(std::string name)
{
    if (applying)
        return;

    if (activeTransaction.has_value)
    {
        if (!activeTransactionIsImplicit)
            return;

        EndTransaction();
    }

    activeTransaction.has_value = true;
    activeTransactionIsImplicit = false;
    activeTransaction.name = std::move(name);
    activeTransaction.before.clear();
    activeTransaction.beforeSelection = CaptureSelection();
}

bool UndoManager::EnsureTransaction(std::string name)
{
    if (applying)
        return false;

    if (activeTransaction.has_value)
        return true;

    activeTransaction.has_value = true;
    activeTransactionIsImplicit = true;
    activeTransaction.name = std::move(name);
    activeTransaction.before.clear();
    activeTransaction.beforeSelection = CaptureSelection();
    return true;
}

void UndoManager::TrackGameObject(GameObject* gameObject, bool includeChildren)
{
    if (gameObject == nullptr || !EnsureTransaction("Inspector Edit"))
        return;

    std::vector<GameObject*> gameObjects = includeChildren ? CollectHierarchy(gameObject) : std::vector<GameObject*>{gameObject};
    for (GameObject* current : gameObjects)
    {
        if (current == nullptr)
            continue;

        auto key = GetEntityKey(UndoEntityKind::GameObject, current->GetUUID());
        if (!activeTransaction.before.contains(key))
            activeTransaction.before.emplace(std::move(key), CaptureGameObjectState(current));
    }
}

void UndoManager::TrackGameObjectHierarchyPlacement(GameObject* gameObject)
{
    if (gameObject == nullptr || !EnsureTransaction("Transform GameObject"))
        return;

    auto key = GetEntityKey(UndoEntityKind::GameObject, gameObject->GetUUID());
    if (!activeTransaction.before.contains(key))
        activeTransaction.before.emplace(std::move(key), CaptureGameObjectHierarchyPlacementState(gameObject));
}

void UndoManager::TrackCreatedGameObject(GameObject* gameObject, bool includeChildren)
{
    if (!activeTransaction.has_value || gameObject == nullptr)
        return;

    std::vector<GameObject*> gameObjects = includeChildren ? CollectHierarchy(gameObject) : std::vector<GameObject*>{gameObject};
    for (GameObject* current : gameObjects)
    {
        if (current == nullptr)
            continue;

        auto key = GetEntityKey(UndoEntityKind::GameObject, current->GetUUID());
        if (!activeTransaction.before.contains(key))
            activeTransaction.before.emplace(std::move(key), CaptureMissingGameObjectState(current));
    }
}

void UndoManager::TrackAsset(Asset* asset)
{
    if (asset == nullptr || !EnsureTransaction("Inspector Edit"))
        return;

    auto key = GetEntityKey(UndoEntityKind::Asset, asset->GetUUID());
    if (!activeTransaction.before.contains(key))
        activeTransaction.before.emplace(std::move(key), CaptureAssetState(asset));
}

void UndoManager::TrackCreatedAsset(Asset* asset)
{
    if (!activeTransaction.has_value || asset == nullptr)
        return;

    auto key = GetEntityKey(UndoEntityKind::Asset, asset->GetUUID());
    if (!activeTransaction.before.contains(key))
        activeTransaction.before.emplace(std::move(key), CaptureMissingAssetState(asset));
}

void UndoManager::EndTransaction()
{
    if (!activeTransaction.has_value)
        return;

    std::unordered_map<std::string, UndoEntityState> after;
    for (auto& [key, beforeState] : activeTransaction.before)
    {
        if (beforeState.kind == UndoEntityKind::GameObject)
        {
            ObjPtr<GameObject> gameObject(beforeState.uuid);
            if (GameObject* current = gameObject.Get())
            {
                after.emplace(
                    key,
                    beforeState.gameObjectMode == GameObjectUndoMode::HierarchyPlacement
                        ? CaptureGameObjectHierarchyPlacementState(current)
                        : CaptureGameObjectState(current)
                );
            }
            else
                after.emplace(key, CaptureMissingGameObjectState(beforeState.uuid, beforeState.typeID, beforeState.scene.Get()));
        }
        else
        {
            Asset* asset = AssetDatabase::Singleton() ? AssetDatabase::Singleton()->LoadAssetByID(beforeState.uuid) : nullptr;
            if (asset)
                after.emplace(key, CaptureAssetState(asset));
            else
                after.emplace(key, CaptureMissingAssetState(beforeState.uuid, beforeState.typeID, beforeState.assetPath));
        }
    }

    PushTransaction(
        std::move(activeTransaction.name),
        std::move(activeTransaction.before),
        std::move(after),
        std::move(activeTransaction.beforeSelection),
        CaptureSelection()
    );
    activeTransaction = {};
    activeTransactionIsImplicit = false;
}

void UndoManager::CommitImplicitTransaction()
{
    if (!activeTransaction.has_value || !activeTransactionIsImplicit)
        return;

    EndTransaction();
}

void UndoManager::CancelTransaction()
{
    activeTransaction = {};
    activeTransactionIsImplicit = false;
}

void UndoManager::CaptureGameObjectChange(
    const std::string& name,
    const std::vector<GameObject*>& gameObjects,
    const std::function<void()>& change,
    bool includeChildren
)
{
    if (change == nullptr)
        return;

    if (applying)
    {
        change();
        return;
    }

    BeginTransaction(name);
    for (GameObject* gameObject : gameObjects)
        TrackGameObject(gameObject, includeChildren);
    change();
    EndTransaction();
}

void UndoManager::CaptureGameObjectCreation(
    const std::string& name,
    const std::function<std::vector<GameObject*>()>& change,
    bool includeChildren
)
{
    if (change == nullptr)
        return;

    if (applying)
    {
        change();
        return;
    }

    BeginTransaction(name);
    std::vector<GameObject*> createdGameObjects = change();
    for (GameObject* gameObject : createdGameObjects)
        TrackCreatedGameObject(gameObject, includeChildren);
    EndTransaction();
}

void UndoManager::CaptureGameObjectDeletion(
    const std::string& name,
    const std::vector<GameObject*>& gameObjects,
    const std::function<void()>& change,
    bool includeChildren
)
{
    if (change == nullptr)
        return;

    if (applying)
    {
        change();
        return;
    }

    BeginTransaction(name);
    for (GameObject* gameObject : gameObjects)
        TrackGameObject(gameObject, includeChildren);
    change();
    EndTransaction();
}

void UndoManager::CaptureGameObjectHierarchyChange(
    const std::string& name,
    const std::vector<GameObject*>& gameObjects,
    const std::function<void()>& change
)
{
    if (change == nullptr)
        return;

    if (applying)
    {
        change();
        return;
    }

    BeginTransaction(name);
    for (GameObject* gameObject : gameObjects)
        TrackGameObjectHierarchyPlacement(gameObject);
    change();
    EndTransaction();
}

void UndoManager::CaptureAssetChange(const std::string& name, const std::vector<Asset*>& assets, const std::function<void()>& change)
{
    if (change == nullptr)
        return;

    if (applying)
    {
        change();
        return;
    }

    BeginTransaction(name);
    for (Asset* asset : assets)
        TrackAsset(asset);
    change();
    EndTransaction();
}

void UndoManager::Push(std::unique_ptr<UndoCommand>&& command)
{
    if (command == nullptr)
        return;

    undoStack.push_back(std::move(command));
    redoStack.clear();

    while (undoStack.size() > maxHistory)
        undoStack.pop_front();
}

std::string UndoManager::GetEntityKey(UndoEntityKind kind, const UUID& uuid)
{
    return fmt::format("{}:{}", static_cast<int>(kind), uuid.ToString());
}

UndoManager::UndoEntityState UndoManager::CaptureGameObjectState(GameObject* gameObject)
{
    UndoEntityState state;
    state.kind = UndoEntityKind::GameObject;
    if (gameObject == nullptr)
        return state;

    state.uuid = gameObject->GetUUID();
    state.typeID = gameObject->GetObjectTypeID();
    state.exists = true;
    state.scene = gameObject->GetScene();
    state.gameObjectMode = GameObjectUndoMode::FullObject;
    CaptureGameObjectPlacement(gameObject, state);

    JsonSerializer serializer;
    gameObject->Serialize(&serializer);
    state.data = serializer.GetJson();
    return state;
}

UndoManager::UndoEntityState UndoManager::CaptureGameObjectHierarchyPlacementState(GameObject* gameObject)
{
    UndoEntityState state;
    state.kind = UndoEntityKind::GameObject;
    if (gameObject == nullptr)
        return state;

    state.uuid = gameObject->GetUUID();
    state.typeID = gameObject->GetObjectTypeID();
    state.exists = true;
    state.scene = gameObject->GetScene();
    state.gameObjectMode = GameObjectUndoMode::HierarchyPlacement;
    CaptureGameObjectPlacement(gameObject, state);
    return state;
}

UndoManager::UndoEntityState UndoManager::CaptureMissingGameObjectState(GameObject* gameObject)
{
    UndoEntityState state = CaptureMissingGameObjectState(
        gameObject ? gameObject->GetUUID() : UUID::GetEmptyUUID(),
        gameObject ? gameObject->GetObjectTypeID() : UUID::GetEmptyUUID(),
        gameObject ? gameObject->GetScene() : nullptr
    );
    CaptureGameObjectPlacement(gameObject, state);
    return state;
}

UndoManager::UndoEntityState UndoManager::CaptureMissingGameObjectState(
    const UUID& uuid,
    const ObjectTypeID& typeID,
    Scene* scene
)
{
    UndoEntityState state;
    state.kind = UndoEntityKind::GameObject;
    state.uuid = uuid;
    state.typeID = typeID;
    state.exists = false;
    state.scene = scene;
    state.gameObjectMode = GameObjectUndoMode::FullObject;
    return state;
}

void UndoManager::CaptureGameObjectPlacement(GameObject* gameObject, UndoEntityState& state)
{
    if (gameObject == nullptr)
        return;

    GameObject* parent = gameObject->GetParent();
    state.isRoot = parent == nullptr;
    state.parentUUID = parent ? parent->GetUUID() : UUID::GetEmptyUUID();
    state.siblingIndex = -1;

    if (parent != nullptr)
    {
        const auto& siblings = parent->GetChildren();
        auto iter = std::find_if(siblings.begin(), siblings.end(), [gameObject](const ObjPtr<GameObject>& child)
                                 { return child.Get() == gameObject; });
        if (iter != siblings.end())
            state.siblingIndex = static_cast<int>(std::distance(siblings.begin(), iter));
    }
    else
    {
        Scene* scene = gameObject->GetScene();
        if (scene != nullptr)
        {
            const auto& roots = scene->GetRootObjects();
            auto iter = std::find_if(roots.begin(), roots.end(), [gameObject](const ObjPtr<GameObject>& root)
                                     { return root.Get() == gameObject; });
            if (iter != roots.end())
                state.siblingIndex = static_cast<int>(std::distance(roots.begin(), iter));
        }
    }

    state.localPosition = gameObject->GetLocalPosition();
    state.localRotation = gameObject->GetLocalRotation();
    state.localScale = gameObject->GetLocalScale();
}

UndoManager::UndoEntityState UndoManager::CaptureAssetState(Asset* asset)
{
    UndoEntityState state;
    state.kind = UndoEntityKind::Asset;
    if (asset == nullptr)
        return state;

    state.uuid = asset->GetUUID();
    state.typeID = asset->GetObjectTypeID();
    state.exists = true;
    if (AssetDatabase::Singleton())
        state.assetPath = AssetDatabase::Singleton()->GetAssetPath(asset->GetUUID());

    JsonSerializer serializer;
    asset->Serialize(&serializer);
    state.data = serializer.GetJson();
    return state;
}

UndoManager::UndoEntityState UndoManager::CaptureMissingAssetState(Asset* asset)
{
    AssetPath assetPath;
    if (asset != nullptr && AssetDatabase::Singleton())
        assetPath = AssetDatabase::Singleton()->GetAssetPath(asset->GetUUID());

    return CaptureMissingAssetState(
        asset ? asset->GetUUID() : UUID::GetEmptyUUID(),
        asset ? asset->GetObjectTypeID() : UUID::GetEmptyUUID(),
        assetPath
    );
}

UndoManager::UndoEntityState UndoManager::CaptureMissingAssetState(
    const UUID& uuid,
    const ObjectTypeID& typeID,
    const AssetPath& assetPath
)
{
    UndoEntityState state;
    state.kind = UndoEntityKind::Asset;
    state.uuid = uuid;
    state.typeID = typeID;
    state.exists = false;
    state.assetPath = assetPath;
    return state;
}

std::vector<GameObject*> UndoManager::CollectHierarchy(GameObject* gameObject)
{
    std::vector<GameObject*> gameObjects;
    if (gameObject == nullptr)
        return gameObjects;

    gameObjects.push_back(gameObject);
    for (GameObject* child : gameObject->GetChildren())
    {
        auto children = CollectHierarchy(child);
        gameObjects.insert(gameObjects.end(), children.begin(), children.end());
    }
    return gameObjects;
}

void UndoManager::PushTransaction(
    std::string name,
    std::unordered_map<std::string, UndoEntityState>&& before,
    std::unordered_map<std::string, UndoEntityState>&& after,
    std::vector<UUID>&& beforeSelection,
    std::vector<UUID>&& afterSelection
)
{
    std::unordered_set<std::string> keys;
    keys.reserve(before.size() + after.size());
    for (auto& [key, _] : before)
        keys.insert(key);
    for (auto& [key, _] : after)
        keys.insert(key);

    std::vector<UndoEntityRecord> records;
    records.reserve(keys.size());
    for (const auto& key : keys)
    {
        auto beforeIter = before.find(key);
        auto afterIter = after.find(key);

        UndoEntityState beforeState;
        UndoEntityState afterState;
        if (beforeIter != before.end())
            beforeState = beforeIter->second;
        if (afterIter != after.end())
            afterState = afterIter->second;

        if (beforeIter == before.end() && afterIter != after.end())
        {
            beforeState = afterState.kind == UndoEntityKind::GameObject
                ? CaptureMissingGameObjectState(afterState.uuid, afterState.typeID, afterState.scene.Get())
                : CaptureMissingAssetState(afterState.uuid, afterState.typeID, afterState.assetPath);
            beforeState.gameObjectMode = afterState.gameObjectMode;
            beforeState.parentUUID = afterState.parentUUID;
            beforeState.siblingIndex = afterState.siblingIndex;
            beforeState.isRoot = afterState.isRoot;
            beforeState.localPosition = afterState.localPosition;
            beforeState.localRotation = afterState.localRotation;
            beforeState.localScale = afterState.localScale;
        }
        else if (afterIter == after.end() && beforeIter != before.end())
        {
            afterState = beforeState.kind == UndoEntityKind::GameObject
                ? CaptureMissingGameObjectState(beforeState.uuid, beforeState.typeID, beforeState.scene.Get())
                : CaptureMissingAssetState(beforeState.uuid, beforeState.typeID, beforeState.assetPath);
            afterState.gameObjectMode = beforeState.gameObjectMode;
            afterState.parentUUID = beforeState.parentUUID;
            afterState.siblingIndex = beforeState.siblingIndex;
            afterState.isRoot = beforeState.isRoot;
            afterState.localPosition = beforeState.localPosition;
            afterState.localRotation = beforeState.localRotation;
            afterState.localScale = beforeState.localScale;
        }

        if (HasStateChanged(beforeState, afterState))
            records.push_back({std::move(beforeState), std::move(afterState)});
    }

    if (records.empty())
        return;

    for (auto& record : records)
    {
        if (record.before.kind == UndoEntityKind::GameObject)
        {
            if (Scene* scene = record.before.scene.Get())
                scene->SetDirty(true);
            if (Scene* scene = record.after.scene.Get())
                scene->SetDirty(true);
        }
        else if (Asset* asset = AssetDatabase::Singleton() ? AssetDatabase::Singleton()->LoadAssetByID(record.before.uuid) : nullptr)
        {
            asset->SetDirty(true);
        }
    }

    Push(std::make_unique<TransactionCommand>(
        *this,
        std::move(name),
        std::move(records),
        std::move(beforeSelection),
        std::move(afterSelection)
    ));
}

void UndoManager::ApplyTransactionState(const std::vector<UndoEntityState>& states)
{
    std::unordered_set<Scene*> touchedScenes;

    for (const UndoEntityState& state : states)
    {
        if (state.kind != UndoEntityKind::Asset)
            continue;

        AssetDatabase* assetDatabase = AssetDatabase::Singleton();
        if (assetDatabase == nullptr)
            continue;

        Asset* asset = assetDatabase->LoadAssetByID(state.uuid);
        if (!state.exists)
        {
            if (!state.assetPath.empty())
                assetDatabase->Remove(state.assetPath);
            continue;
        }

        if (asset == nullptr)
        {
            auto newAsset = AssetRegistry::CreateAsset(state.typeID);
            if (newAsset == nullptr)
            {
                spdlog::warn("failed to recreate undo asset {}", state.uuid.ToString());
                continue;
            }

            JsonSerializer serializer(state.data);
            newAsset->Deserialize(&serializer);
            newAsset->OnLoaded();
            asset = assetDatabase->SaveAsset(std::move(newAsset), state.assetPath);
        }
        else
        {
            JsonSerializer serializer(state.data);
            asset->Deserialize(&serializer);
            asset->OnLoaded();
        }

        if (asset != nullptr)
            asset->SetDirty(true);
    }

    std::vector<const UndoEntityState*> gameObjectStates;
    gameObjectStates.reserve(states.size());
    for (const UndoEntityState& state : states)
    {
        if (state.kind == UndoEntityKind::GameObject)
            gameObjectStates.push_back(&state);
    }

    std::unordered_map<std::string, const UndoEntityState*> gameObjectStateByUUID;
    for (const UndoEntityState* state : gameObjectStates)
        gameObjectStateByUUID[state->uuid.ToString()] = state;

    auto getDepth = [&](const UndoEntityState& state) {
        int depth = 0;
        UUID parentUUID = state.parentUUID;
        std::unordered_set<std::string> visited;
        while (!parentUUID.IsEmpty() && visited.insert(parentUUID.ToString()).second)
        {
            auto iter = gameObjectStateByUUID.find(parentUUID.ToString());
            if (iter == gameObjectStateByUUID.end())
                break;

            ++depth;
            parentUUID = iter->second->parentUUID;
        }
        return depth;
    };

    auto sortShallowFirst = [&](std::vector<const UndoEntityState*>& orderedStates) {
        std::sort(orderedStates.begin(), orderedStates.end(), [&](const UndoEntityState* lhs, const UndoEntityState* rhs) {
            return getDepth(*lhs) < getDepth(*rhs);
        });
    };

    auto sortDeepFirst = [&](std::vector<const UndoEntityState*>& orderedStates) {
        std::sort(orderedStates.begin(), orderedStates.end(), [&](const UndoEntityState* lhs, const UndoEntityState* rhs) {
            return getDepth(*lhs) > getDepth(*rhs);
        });
    };

    auto applyHierarchyPlacement = [&](GameObject* gameObject, const UndoEntityState& state) {
        if (gameObject == nullptr)
            return;

        Scene* scene = state.scene.Get();
        if (scene == nullptr)
            scene = gameObject->GetScene();
        if (scene == nullptr)
            return;

        ObjPtr<GameObject> parentPtr(state.parentUUID);
        GameObject* parent = state.isRoot ? nullptr : parentPtr.Get();
        if (parent != nullptr)
        {
            gameObject->SetParent(parent, false);
            parent->InsertChild(gameObject, state.siblingIndex);
        }
        else
        {
            if (gameObject->GetParent() != nullptr)
                gameObject->SetParent(nullptr, false);

            scene->RemoveGameObjectFromRoot(gameObject);
            scene->MoveGameObjectToRoot(gameObject);
            scene->MoveRootGameObjectToIndex(gameObject, state.siblingIndex);
        }

        gameObject->SetLocalPosition(state.localPosition);
        gameObject->SetLocalRotation(state.localRotation);
        gameObject->SetLocalScale(state.localScale);

        touchedScenes.insert(scene);
        scene->SetDirty(true);
    };

    std::vector<const UndoEntityState*> fullDeleteStates;
    std::vector<const UndoEntityState*> fullRestoreStates;
    std::vector<const UndoEntityState*> hierarchyStates;
    std::unordered_set<UUID> fullObjectStateUUIDs;
    for (const UndoEntityState* state : gameObjectStates)
    {
        if (state->gameObjectMode == GameObjectUndoMode::HierarchyPlacement)
        {
            hierarchyStates.push_back(state);
        }
        else if (state->exists)
        {
            fullRestoreStates.push_back(state);
            fullObjectStateUUIDs.insert(state->uuid);
        }
        else
        {
            fullDeleteStates.push_back(state);
            fullObjectStateUUIDs.insert(state->uuid);
        }
    }

    sortDeepFirst(fullDeleteStates);
    for (const UndoEntityState* state : fullDeleteStates)
    {
        ObjPtr<GameObject> gameObjectPtr(state->uuid);
        GameObject* gameObject = gameObjectPtr.Get();
        if (gameObject == nullptr || gameObject->GetScene() == nullptr)
            continue;

        touchedScenes.insert(gameObject->GetScene());
        gameObject->GetScene()->DestroyGameObject(gameObject);
    }

    struct PreservedChild
    {
        ObjPtr<GameObject> child;
    };
    std::unordered_map<UUID, std::vector<PreservedChild>, std::hash<UUID>> preservedChildrenByParent;

    sortDeepFirst(fullRestoreStates);
    for (const UndoEntityState* state : fullRestoreStates)
    {
        ObjPtr<GameObject> gameObjectPtr(state->uuid);
        GameObject* gameObject = gameObjectPtr.Get();
        if (gameObject == nullptr)
            continue;

        Scene* scene = state->scene.Get();
        if (scene == nullptr)
            scene = gameObject->GetScene();
        if (scene == nullptr)
            continue;

        std::vector<ObjPtr<GameObject>> children(gameObject->GetChildren().begin(), gameObject->GetChildren().end());
        auto& preservedChildren = preservedChildrenByParent[state->uuid];
        for (const ObjPtr<GameObject>& childPtr : children)
        {
            GameObject* child = childPtr.Get();
            if (child == nullptr || fullObjectStateUUIDs.contains(child->GetUUID()))
                continue;

            preservedChildren.push_back({child});
            child->SetParent(nullptr, true);
        }

        touchedScenes.insert(scene);
        scene->DestroyGameObject(gameObject);
    }

    sortShallowFirst(fullRestoreStates);
    for (const UndoEntityState* state : fullRestoreStates)
    {
        ObjPtr<GameObject> gameObjectPtr(state->uuid);
        GameObject* gameObject = gameObjectPtr.Get();
        Scene* scene = state->scene.Get();
        if (scene == nullptr)
            scene = gameObject ? gameObject->GetScene() : SceneManager::GetActiveScene();
        if (scene == nullptr)
            continue;

        if (gameObject == nullptr)
        {
            auto object = ObjectRegistry::CreateObject(state->typeID);
            GameObject* createdGameObject = dynamic_cast<GameObject*>(object.get());
            std::unique_ptr<GameObject> newGameObject;
            if (createdGameObject != nullptr)
            {
                object.release();
                newGameObject.reset(createdGameObject);
            }
            if (newGameObject == nullptr)
            {
                spdlog::warn("failed to recreate undo GameObject {}", state->uuid.ToString());
                continue;
            }

            JsonSerializer serializer(state->data);
            newGameObject->Deserialize(&serializer);
            newGameObject->OnLoaded();
            gameObject = newGameObject.get();
            scene->AddGameObject(std::move(newGameObject));
        }

        applyHierarchyPlacement(gameObject, *state);

        auto preservedIter = preservedChildrenByParent.find(state->uuid);
        if (preservedIter != preservedChildrenByParent.end())
        {
            std::vector<UUID> restoredChildUUIDs = GetSerializedChildUUIDs(state->data);
            int restoredChildIndex = 0;
            for (const UUID& childUUID : restoredChildUUIDs)
            {
                auto childIter = std::find_if(
                    preservedIter->second.begin(),
                    preservedIter->second.end(),
                    [&childUUID](const PreservedChild& preservedChild)
                    {
                        GameObject* child = preservedChild.child.Get();
                        return child != nullptr && child->GetUUID() == childUUID;
                    }
                );

                if (childIter == preservedIter->second.end())
                {
                    ++restoredChildIndex;
                    continue;
                }

                if (GameObject* child = childIter->child.Get())
                {
                    child->SetParent(gameObject, false);
                    gameObject->MoveChildToIndex(child, restoredChildIndex);
                }
                ++restoredChildIndex;
            }
        }
    }

    sortShallowFirst(hierarchyStates);
    for (const UndoEntityState* state : hierarchyStates)
    {
        if (!state->exists)
            continue;

        ObjPtr<GameObject> gameObjectPtr(state->uuid);
        applyHierarchyPlacement(gameObjectPtr.Get(), *state);
    }

    for (Scene* scene : touchedScenes)
    {
        if (scene != nullptr)
        {
            scene->SetDirty(true);
        }
    }
}

std::vector<UUID> UndoManager::CaptureSelection()
{
    std::vector<UUID> selection;
    auto selectedObjects = EditorState::GetSelectedObjects();
    selection.reserve(selectedObjects.size());
    for (const auto& selected : selectedObjects)
    {
        if (Object* obj = selected.Get())
            selection.push_back(obj->GetUUID());
    }
    return selection;
}

void UndoManager::RestoreSelection(const std::vector<UUID>& selection)
{
    EditorState::SelectObject(nullptr);
    for (const UUID& uuid : selection)
    {
        ObjPtr<Object> object(uuid);
        if (object.Get())
            EditorState::SelectObject(object, true);
    }
}
} // namespace Editor
