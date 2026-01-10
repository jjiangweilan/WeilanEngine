#pragma once

#include "Engine/Core/Asset.hpp"
#include "Engine/Core/EngineState.hpp"
#include "Engine/Core/Ptr.hpp"
#include "Engine/Library/DynamicArray.hpp"
#include "Engine/Runtime/Object/Component/Component.hpp"
#include "Engine/Runtime/Object/GameObject/Prefab.hpp"
#include <functional>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <memory>

// clang-format off
#include <Jolt/Jolt.h>
// clang-format on
#include <Jolt/Physics/Collision/ContactListener.h>

class Scene;

struct lua_State;
enum class RotationCoordinate
{
    Self,
    Parent,
    World,
};

enum class GameObjectFlag : uint32_t
{
    None = 0,
    DontChangeHierarchy
};
ENUM_FLAGS(GameObjectFlag, uint32_t);

class Prefab;
class PhysicsBody;
using PhysicsContactCallback =
    std::function<void(PhysicsBody*, PhysicsBody*, const JPH::ContactManifold&, JPH::ContactSettings&)>;

class [[TrClass, LuaClass]] GameObject : public Object
{
    DECLARE_OBJECT();

    // Prefab & Flags
    ObjPtr<Prefab> prefab [[TrProp]] = nullptr;
    GameObjectFlag flags = GameObjectFlag::None;

    // Transform data
    float3 position [[TrProp]] = float3(0);
    float3 scale [[TrProp]] = float3(1, 1, 1);
    glm::quat rotation [[TrProp]] = glm::quat(1, 0, 0, 0);
    float3 eulerAngles = float3(0, 0, 0);
    mutable float4x4 localMatrix;
    mutable float4x4 worldMatrix;

    // State flags
    bool enabled [[TrProp]] = false;
    bool wantsToBeEnabled = false;
    mutable bool transformChanged = true;
    mutable bool updateLocalMatrix = true;

    // Physics callbacks
    std::vector<PhysicsContactCallback> contactAddedCallbacks = {};
    std::vector<PhysicsContactCallback> contactRemovedCallbacks = {};

    // Hierarchy & Components
    std::vector<ObjPtr<GameObject>> children [[TrProp]];
    std::vector<std::unique_ptr<GameObject>> owningChildren;
    std::vector<std::unique_ptr<Component>> components [[TrProp]];
    std::vector<std::unique_ptr<Component>> prefabComponents;
    std::vector<Component*> allComponents;
    ObjPtr<GameObject> parent [[TrProp]] = nullptr;
    ObjPtr<Scene> gameScene = nullptr;
    bool isAwaked = false;

    inline static const float compareEpsilon = 1e-6f;

public:
    // Constructors & Destructor
    GameObject();
    GameObject(Scene* gameScene);
    GameObject(GameObject&& other);
    GameObject(const GameObject& other);
    ~GameObject();

    // Component Management
    template <class T, class... Args>
    T* AddComponent(Args&&... args);
    Component* AddComponent(std::string_view componentName);
    [[LuaNamedFn("AddComponent")]] Component* Lua_AddComponent(const char* componentName) { return AddComponent(std::string_view(componentName)); }
    void RemoveComponent(void* comp);
    void MoveInComponent(Component* source);
    void RemoveComponentByIndex(int componentIndex);

    template <class T>
    T* GetComponent();
    ObjPtr<Component> GetComponent(const char* className);
    [[LuaFn]] ObjPtr<Component> GetComponentInHierarchy(const char* className);
    std::span<Component*> GetComponents();

    template <class T>
    std::vector<T*> GetComponentsInChildren();

    [[LuaNamedFn("SetName")]] void Lua_SetName(const char* name) { SetName(name); }
    [[LuaNamedFn("GetName")]] std::string Lua_GetName() const { return GetName(); }

    // Scene & Lifecycle
    void SetScene(Scene* scene);
    [[LuaFn]] Scene* GetScene();
    void Tick();
    void IdleTick();
    void PrePhysicsTick();
    void DebugDraw();

    // Serialization
    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    void OnLoaded();

    // Hierarchy Management
    GameObject* GetParent() const { return parent; }
    void SetParent(GameObject* parent, bool keepWorldSpacePostion = true);
    const std::vector<ObjPtr<GameObject>>& GetChildren() { return children; }
    void RemoveChild(GameObject* child);
    auto GetOwningChildren() { return std::move(owningChildren); }
    GameObject* Find(std::string_view name);

    // Enable/Disable State
    [[LuaFn]] bool IsEnabled() { return enabled; }
    [[LuaFn]] bool IsActiveInScene() const { return enabled && (parent != nullptr ? parent->IsActiveInScene() : true); }
    [[LuaFn]] void SetEnable(bool isEnabled);
    void SetWantsToBeEnabled() { wantsToBeEnabled = true; }
    bool GetWantsTobeEnabledStateAndReset();
    bool WantsTobeEnabled() { return wantsToBeEnabled; }

    // Transform - Position
    [[LuaFn]] float3 GetPosition() const;
    [[LuaFn]] float3 GetLocalPosition() const { return position; }
    [[LuaFn]] void SetPosition(const float3& position);
    [[LuaFn]] void SetLocalPosition(const float3& localPosition);
    void Translate(const float3& translate);

    // Transform - Rotation
    [[LuaFn]] glm::quat GetRotation() const;
    [[LuaFn]] glm::quat GetLocalRotation() const { return rotation; }
    [[LuaFn]] void SetRotation(const glm::quat& rotation);
    [[LuaFn]] void SetLocalRotation(const glm::quat& rotation);
    [[LuaFn]] float3 GetEuluerAngles() const { return eulerAngles; }
    [[LuaFn]] void SetEulerAngles(const float3& eulerAngles);
    void Rotate(const float3& axis, float angle, RotationCoordinate coord = RotationCoordinate::Self);
    void Rotate(glm::quat quaternion);
    void RotateAround(const float3& point, const float3& axis, float angle);
    [[LuaFn]] void LookAt(const float3& to);

    // Transform - Scale
    [[LuaFn]] float3 GetScale() const;
    [[LuaFn]] float3 GetLocalScale() const { return scale; }
    [[LuaFn]] void SetScale(const float3& scale);
    [[LuaFn]] void SetLocalScale(const float3& scale);

    // Transform - Direction Vectors
    [[LuaFn]] float3 GetForward() const;
    [[LuaFn]] float3 GetUp() const;
    [[LuaFn]] float3 GetRight() const;

    // Transform - Matrices
    [[LuaFn]] float4x4 GetWorldMatrix() const;
    [[LuaFn]] const float4x4& GetLocalMatrix() const;
    [[LuaFn]] void SetWorldMatrix(const float4x4& model);
    void ResetTransform();

    // Prefab
    void LinkPrefab(Prefab* prefab);
    bool HasPrefab() const { return prefab != nullptr; }
    auto GetPrefab() const { return prefab; }
    void ResetToPrefab();
    void ApplyToPrefab();

    // Flags
    void SetFlags(GameObjectFlag f) { this->flags = f; }
    GameObjectFlag GetFlags() { return flags; }

    // Physics Callbacks
    int RegisterContactEventAdded(const PhysicsContactCallback& f);
    int RegisterContactEventRemoved(const PhysicsContactCallback& f);
    void UnregisterContactEventAdded(int id);
    void UnregisterContactEventRemoved(int id);
    void OnContactAdded(
        PhysicsBody* body1, PhysicsBody* body2, const JPH::ContactManifold& manifold, JPH::ContactSettings& settings
    );
    void OnContactRemoved(
        PhysicsBody* body1, PhysicsBody* body2, const JPH::ContactManifold& manifold, JPH::ContactSettings& settings
    );

    // Lifecycle Callbacks
    void OnStart();
    void OnAwake();
    void OnStop();

    [[LuaRawFn("GetComponent")]]
    static int LuaGetComponent(lua_State* L);

    void Copy(const GameObject& other, bool withComponent = true);
private:
    GameObject* FindInternal(GameObject* go, std::string_view name);

    inline bool EqualZero(const float3& v)
    {
        return glm::abs(v.x) < compareEpsilon && glm::abs(v.y) < compareEpsilon && glm::abs(v.z) < compareEpsilon;
    }

    void TransformChanged();

    void UpdateAllComponents();
    void ApplyPrefabComponents();

    friend void RegisterSerializedObjects();
};

template <class T, class... Args>
T* GameObject::AddComponent(Args&&... args)
{
    auto p = std::make_unique<T>(this, args...);
    T* temp = p.get();
    components.push_back(std::move(p));
    temp->OnInit();
    temp->Enable();

    UpdateAllComponents();
    return temp;
}

template <class T>
T* GameObject::GetComponent()
{
    for (auto& p : components)
    {
        T* cast = dynamic_cast<T*>(p.get());
        if (cast != nullptr)
            return cast;
    }
    return nullptr;
}

template <class T>
std::vector<T*> GameObject::GetComponentsInChildren()
{
    std::vector<T*> results;

    if (auto comp = GetComponent<T>())
    {
        results.push_back(comp);
    }

    for (auto c : children)
    {
        if (c == nullptr)
            continue;
        std::vector<T*> cs = c->GetComponentsInChildren<T>();
        results.insert(results.end(), cs.begin(), cs.end());
    }

    return results;
}
