#pragma once

#include "Engine/WeilanEngineAPI.hpp"
#include "Engine/Core/Asset.hpp"
#include "Engine/Core/Ptr.hpp"
#include "Engine/Library/Serialization/JsonSerializer.hpp"
#include "Engine/Library/UUID.hpp"
#include "Engine/Runtime/Object/GameObject/GameObject.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetPath.hpp"
#include "Engine/Runtime/System/SceneManager/Scene.hpp"
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Editor
{
class WEILAN_ENGINE_API UndoCommand
{
public:
    virtual ~UndoCommand() = default;
    virtual void Undo() = 0;
    virtual void Redo() = 0;
    virtual const std::string& GetName() const = 0;
};

class WEILAN_ENGINE_API UndoManager
{
public:
    static constexpr size_t DefaultMaxHistory = 256;

    UndoManager() = default;
    UndoManager(const UndoManager&) = delete;
    UndoManager& operator=(const UndoManager&) = delete;
    UndoManager(UndoManager&&) = delete;
    UndoManager& operator=(UndoManager&&) = delete;

    void Execute(std::unique_ptr<UndoCommand>&& command);
    void Undo();
    void Redo();
    void Clear();

    bool CanUndo() const { return !undoStack.empty(); }
    bool CanRedo() const { return !redoStack.empty(); }
    bool IsApplying() const { return applying; }

    const std::string& GetUndoName() const;
    const std::string& GetRedoName() const;

    void SetMaxHistory(size_t count) { maxHistory = count; }

    void BeginTransaction(std::string name);
    void TrackGameObject(GameObject* gameObject, bool includeChildren = false);
    void TrackGameObjectHierarchyPlacement(GameObject* gameObject);
    void TrackCreatedGameObject(GameObject* gameObject, bool includeChildren = false);
    void TrackAsset(Asset* asset);
    void TrackCreatedAsset(Asset* asset);
    void EndTransaction();
    void CommitImplicitTransaction();
    void CancelTransaction();

    void CaptureGameObjectChange(
        const std::string& name,
        const std::vector<GameObject*>& gameObjects,
        const std::function<void()>& change,
        bool includeChildren = false
    );
    void CaptureGameObjectCreation(
        const std::string& name,
        const std::function<std::vector<GameObject*>()>& change,
        bool includeChildren = true
    );
    void CaptureGameObjectDeletion(
        const std::string& name,
        const std::vector<GameObject*>& gameObjects,
        const std::function<void()>& change,
        bool includeChildren = true
    );
    void CaptureGameObjectHierarchyChange(
        const std::string& name,
        const std::vector<GameObject*>& gameObjects,
        const std::function<void()>& change
    );
    void CaptureAssetChange(const std::string& name, const std::vector<Asset*>& assets, const std::function<void()>& change);

    bool HasActiveTransaction() const { return activeTransaction.has_value; }

    static void RestoreSelection(const std::vector<UUID>& selection);

    enum class UndoEntityKind
    {
        GameObject,
        Asset,
    };

    enum class GameObjectUndoMode
    {
        FullObject,
        HierarchyPlacement,
    };

    struct UndoEntityState
    {
        UndoEntityKind kind = UndoEntityKind::GameObject;
        UUID uuid = UUID::GetEmptyUUID();
        ObjectTypeID typeID = UUID::GetEmptyUUID();
        bool exists = false;
        nlohmann::json data;

        ObjPtr<Scene> scene;
        AssetPath assetPath;

        GameObjectUndoMode gameObjectMode = GameObjectUndoMode::FullObject;
        UUID parentUUID = UUID::GetEmptyUUID();
        int siblingIndex = -1;
        bool isRoot = false;
        float3 localPosition = float3(0);
        glm::quat localRotation = glm::identity<glm::quat>();
        float3 localScale = float3(1);
    };

    struct UndoEntityRecord
    {
        UndoEntityState before;
        UndoEntityState after;
    };

    void ApplyTransactionState(const std::vector<UndoEntityState>& states);

private:

    struct Transaction
    {
        bool has_value = false;
        std::string name;
        std::unordered_map<std::string, UndoEntityState> before;
        std::vector<UUID> beforeSelection;
    } activeTransaction;

    std::deque<std::unique_ptr<UndoCommand>> undoStack;
    std::deque<std::unique_ptr<UndoCommand>> redoStack;
    size_t maxHistory = DefaultMaxHistory;
    bool applying = false;
    bool activeTransactionIsImplicit = false;

    void Push(std::unique_ptr<UndoCommand>&& command);
    bool EnsureTransaction(std::string name);
    static std::string GetEntityKey(UndoEntityKind kind, const UUID& uuid);
    static UndoEntityState CaptureGameObjectState(GameObject* gameObject);
    static UndoEntityState CaptureGameObjectHierarchyPlacementState(GameObject* gameObject);
    static UndoEntityState CaptureMissingGameObjectState(GameObject* gameObject);
    static UndoEntityState CaptureMissingGameObjectState(const UUID& uuid, const ObjectTypeID& typeID, Scene* scene);
    static void CaptureGameObjectPlacement(GameObject* gameObject, UndoEntityState& state);
    static UndoEntityState CaptureAssetState(Asset* asset);
    static UndoEntityState CaptureMissingAssetState(Asset* asset);
    static UndoEntityState CaptureMissingAssetState(const UUID& uuid, const ObjectTypeID& typeID, const AssetPath& assetPath);
    static std::vector<GameObject*> CollectHierarchy(GameObject* gameObject);
    void PushTransaction(
        std::string name,
        std::unordered_map<std::string, UndoEntityState>&& before,
        std::unordered_map<std::string, UndoEntityState>&& after,
        std::vector<UUID>&& beforeSelection,
        std::vector<UUID>&& afterSelection
    );
    static std::vector<UUID> CaptureSelection();
};
} // namespace Editor
