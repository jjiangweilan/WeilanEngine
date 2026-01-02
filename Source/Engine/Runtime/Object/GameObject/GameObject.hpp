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

class [[TrClass]] GameObject : public Object
{
    DECLARE_OBJECT();

    // Prefab & Flags
    ObjPtr<Prefab> prefab [[TrProp]] = nullptr;
    GameObjectFlag flags = GameObjectFlag::None;

    // Transform data
    glm::vec3 position [[TrProp]] = glm::vec3(0);
    glm::vec3 scale [[TrProp]] = glm::vec3(1, 1, 1);
    glm::quat rotation [[TrProp]] = glm::quat(1, 0, 0, 0);
    glm::vec3 eulerAngles = glm::vec3(0, 0, 0);
    mutable glm::mat4 localMatrix;
    mutable glm::mat4 worldMatrix;

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
    void RemoveComponent(void* comp);
    void MoveInComponent(Component* source);
    void RemoveComponentByIndex(int componentIndex);

    template <class T>
    T* GetComponent();
    ObjPtr<Component> GetComponent(const char* className);
    ObjPtr<Component> GetComponentInHierachy(const char* className);
    std::span<Component*> GetComponents();

    template <class T>
    std::vector<T*> GetComponentsInChildren();

    // Scene & Lifecycle
    void SetScene(Scene* scene);
    Scene* GetScene();
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
    bool IsEnabled() { return enabled; }
    bool IsActiveInScene() const { return enabled && (parent != nullptr ? parent->IsActiveInScene() : true); }
    void SetEnable(bool isEnabled);
    void SetWantsToBeEnabled() { wantsToBeEnabled = true; }
    bool GetWantsTobeEnabledStateAndReset();

    // Transform - Position
    glm::vec3 GetPosition() const;
    glm::vec3 GetLocalPosition() const { return position; }
    void SetPosition(const glm::vec3& position);
    void SetLocalPosition(const glm::vec3& localPosition);
    void Translate(const glm::vec3& translate);

    // Transform - Rotation
    glm::quat GetRotation() const;
    glm::quat GetLocalRotation() const { return rotation; }
    void SetRotation(const glm::quat& rotation);
    void SetLocalRotation(const glm::quat& rotation);
    glm::vec3 GetEuluerAngles() const { return eulerAngles; }
    void SetEulerAngles(const glm::vec3& eulerAngles);
    void Rotate(const glm::vec3& axis, float angle, RotationCoordinate coord = RotationCoordinate::Self);
    void Rotate(glm::quat quaternion);
    void RotateAround(const glm::vec3& point, const glm::vec3& axis, float angle);
    void LookAt(const glm::vec3& to);

    // Transform - Scale
    glm::vec3 GetScale() const;
    glm::vec3 GetLocalScale() const { return scale; }
    void SetScale(const glm::vec3& scale);
    void SetLocalScale(const glm::vec3& scale);

    // Transform - Direction Vectors
    glm::vec3 GetForward() const;
    glm::vec3 GetUp() const;
    glm::vec3 GetRight() const;

    // Transform - Matrices
    glm::mat4 GetWorldMatrix() const;
    const glm::mat4& GetLocalMatrix() const;
    void SetWorldMatrix(const glm::mat4& model);
    void ResetTransform();

    // Prefab
    void LinkPrefab(Prefab* prefab);
    bool HasPrefab() const { return prefab != nullptr; }
    auto GetPrefab() const { return prefab; }
    void ResetToPrefab();

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

private:
    GameObject* FindInternal(GameObject* go, std::string_view name);

    inline bool EqualZero(const glm::vec3& v)
    {
        return glm::abs(v.x) < compareEpsilon && glm::abs(v.y) < compareEpsilon && glm::abs(v.z) < compareEpsilon;
    }

    void TransformChanged();
    void Copy(const GameObject& other);

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
