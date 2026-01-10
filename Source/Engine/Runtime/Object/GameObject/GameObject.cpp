#include "GameObject.hpp"
#include "Engine/Library/Math.hpp"
#include "Engine/Library/TypeReflection.hpp"
#include "Engine/Runtime/Object/Component/GameScript.hpp"
#include "Engine/Runtime/Object/GameObject/Prefab.hpp"
#include "Engine/Runtime/System/SceneManager/Scene.hpp"
#include "Engine/Runtime/System/ScriptingBackend/LuaBindings_Private.hpp"
#include <glm/gtx/matrix_decompose.hpp>
#include <spdlog/spdlog.h>

DEFINE_OBJECT(Object, GameObject, "F04CAB0A-DCF0-4ECF-A690-13FBD63A1AC7");

GameObject::GameObject() : gameScene(nullptr)
{
    name = "New GameObject";
    ResetTransform();
}

GameObject::GameObject(GameObject&& other)
    : components(std::move(other.components)), prefab(std::exchange(other.prefab, nullptr)), gameScene(std::exchange(other.gameScene, nullptr))
{
    SetName(other.GetName());
    ResetTransform();
    ApplyPrefabComponents();
    UpdateAllComponents();
}

GameObject::GameObject(Scene* gameScene) : gameScene(gameScene)
{
    name = "New GameObject";
    ResetTransform();
}

void GameObject::ResetTransform()
{
    position = float3(0);
    scale = float3(1);
    rotation = glm::identity<glm::quat>();
    eulerAngles = float3(0, 0, 0);
}

void GameObject::Copy(const GameObject& other, bool withComponent)
{
    SetName(other.GetName());
    prefab = other.prefab;
    position = other.position;
    scale = other.scale;
    rotation = other.rotation;
    eulerAngles = other.eulerAngles;
    wantsToBeEnabled = other.enabled || other.wantsToBeEnabled;
    gameScene = nullptr;
    enabled = false;

    if (withComponent)
    {
        for (auto& c : other.components)
        {
            components.push_back(c->Clone(*this));
        }
    }

    for (GameObject* child : other.children)
    {
        auto childCopy = std::make_unique<GameObject>();
        childCopy->Copy(*child, withComponent);
        owningChildren.push_back(std::move(childCopy));
        owningChildren.back()->SetParent(this, false);
    }

    ApplyPrefabComponents();
    UpdateAllComponents();
}

GameObject::GameObject(const GameObject& other)
{
    Copy(other);
}

GameObject::~GameObject()
{
    components.clear();
    prefabComponents.clear();
}

void GameObject::Tick()
{
    for (auto& comp : allComponents)
    {
        if (comp && comp->IsEnabled())
            comp->Tick();
    }
}

void GameObject::DebugDraw()
{
    for (auto& comp : allComponents)
    {
        comp->DebugDraw();
    }
}

void GameObject::IdleTick()
{
    for (auto& comp : allComponents)
    {
        if (comp && comp->IsEnabled())
            comp->IdleTick();
    }
}

void GameObject::PrePhysicsTick()
{
    for (auto& comp : allComponents)
    {
        if (comp && comp->IsEnabled())
            comp->PrePhysicsTick();
    }
}

std::span<Component*> GameObject::GetComponents()
{
    return allComponents;
}

Scene* GameObject::GetScene()
{
    return gameScene;
}

void GameObject::Serialize(Serializer* s) const
{
    Object::Serialize(s);
    s->Serialize("components", components);
    s->Serialize("prefabComponents", prefabComponents);
    s->Serialize("rotation", rotation);
    s->Serialize("position", position);
    s->Serialize("scale", scale);
    s->Serialize("parent", parent);
    s->Serialize("children", children);
    s->Serialize("enabled", enabled);
    s->Serialize("prefab", prefab);
    s->Serialize("wantsToBeEnabled", wantsToBeEnabled);
}

void GameObject::SetWorldMatrix(const float4x4& matrix)
{
    auto localMatrix = matrix;
    if (parent)
        localMatrix = glm::inverse(parent->GetWorldMatrix()) * matrix;

    float3 position{};
    float3 scale{};
    glm::quat rotation{};

    Math::DecomposeMatrix(localMatrix, position, scale, rotation);

    SetEulerAngles(glm::eulerAngles(rotation));
    SetLocalScale(scale);
    SetLocalPosition(position);
}

void GameObject::Deserialize(Serializer* s)
{
    Object::Deserialize(s);
    s->Deserialize("enabled", enabled);
    s->Deserialize("children", children);
    s->Deserialize("parent", parent);
    s->Deserialize("scale", scale);
    s->Deserialize("position", position);
    s->Deserialize("rotation", rotation);
    s->Deserialize("components", components);
    s->Deserialize("prefab", prefab);
    s->Deserialize("wantsToBeEnabled", wantsToBeEnabled);
    // gameScene is set by Scene when it's deserializing
}

void GameObject::OnLoaded()
{
    eulerAngles = glm::eulerAngles(rotation);

    ApplyPrefabComponents();
    UpdateAllComponents();

    for (auto& c : allComponents)
    {
        if (c)
            c->gameObject = this;
    }

    for (auto& c : allComponents)
    {
        if (c)
        {
            c->OnInit();
            c->OnLoaded();
        }
    }
}

void GameObject::RemoveChild(GameObject* child)
{
    auto it = children.begin();
    while (it != children.end())
    {
        if ((*it).Get() == child)
        {
            children.erase(it);
            return;
        }
        it += 1;
    }
}

void GameObject::SetParent(GameObject* newParent, bool keepWorldSpacePostion)
{
    if (this->parent.Get() == newParent || HasFlag(flags, GameObjectFlag::DontChangeHierarchy))
    {
        return;
    }

    if (newParent == nullptr)
    {
        Scene* scene = GetScene();
        if (scene)
            scene->MoveGameObjectToRoot(this);
        this->parent->RemoveChild(this);
    }

    if (this->parent == nullptr)
    {
        Scene* scene = GetScene();
        if (scene)
            scene->RemoveGameObjectFromRoot(this);
    }
    else
    {
        this->parent->RemoveChild(this);
    }

    // fix local transforms
    if (keepWorldSpacePostion)
    {
        float4x4 parentWorld = glm::mat4(1);
        if (newParent != nullptr)
        {
            parentWorld = newParent->GetWorldMatrix();
        }
        float4x4 currentWorld = GetWorldMatrix();

        float4x4 local = glm::inverse(parentWorld) * currentWorld;

        float3 newPosition, newScale;
        glm::quat newRotation;
        Math::DecomposeMatrix(local, newPosition, newScale, newRotation);
        SetLocalPosition(newPosition);
        SetEulerAngles(glm::eulerAngles(newRotation));
        SetLocalScale(newScale);
    }

    this->parent = newParent;
    if (newParent)
        newParent->children.push_back(this);
}

void GameObject::SetScene(Scene* scene)
{
    if (this->gameScene.Get() != scene)
    {
        if (this->gameScene != nullptr)
        {
            for (auto& c : allComponents)
            {
                if (c && c->IsEnabled())
                    c->OnDisable();
            }
        }

        this->gameScene = scene;
        for (auto& c : allComponents)
        {
            if (c && c->IsEnabled() && enabled)
                c->OnEnable();
        }

        for (auto child : children)
        {
            if (child != nullptr)
                child->SetScene(scene);
        }
    }
}

void GameObject::SetEnable(bool isEnabled)
{
    wantsToBeEnabled = isEnabled;

    if (enabled == isEnabled || gameScene == nullptr)
        return;

    if (isEnabled)
    {
        for (auto& c : allComponents)
        {
            if (c && c->IsEnabled())
            {
                c->OnEnable();
            }
        }

        if (!isAwaked)
        {
            isAwaked = true;
            for (auto& c : allComponents)
            {
                if (c && c->IsEnabled())
                {
                    c->OnAwake();
                }
            }
        }
    }
    else
    {
        for (auto& c : allComponents)
        {
            if (c && c->IsEnabled())
                c->OnDisable();
        }
    }

    for (auto child : children)
    {
        // child may be nullptr when deserializing
        if (child)
        {
            child->SetEnable(isEnabled);
        }
    }

    enabled = isEnabled;
}

void GameObject::SetScale(const float3& s)
{
    SetLocalScale(s);
}

const float4x4& GameObject::GetLocalMatrix() const
{
    if (updateLocalMatrix)
    {
        localMatrix =
            glm::translate(float4x4(1), position) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1), scale);
        updateLocalMatrix = false;
    }

    return localMatrix;
}

float4x4 GameObject::GetWorldMatrix() const
{
    auto& localMatrix = GetLocalMatrix();

    if (transformChanged)
    {
        if (parent != nullptr)
        {
            worldMatrix = parent->GetWorldMatrix() * localMatrix;
            transformChanged = false;
            return worldMatrix;
        }
        else
        {
            worldMatrix = localMatrix;
        }
    }

    transformChanged = false;
    if (parent == nullptr)
        return worldMatrix;

    return worldMatrix;
}

glm::quat GameObject::GetRotation() const
{
    glm::quat r = rotation;
    if (parent != nullptr)
    {
        r = r * parent->GetRotation();
    }
    return r;
}

void GameObject::SetPosition(const float3& position)
{
    float3 pos = position;
    if (parent != nullptr)
    {
        pos = position - parent->GetPosition();
    }

    if (pos != this->position)
    {
        this->position = pos;
        this->updateLocalMatrix = true;
        TransformChanged();
    }
};

void GameObject::SetLocalRotation(const glm::quat& rotation)
{
    if (rotation == this->rotation)
        return;

    this->rotation = rotation;
    updateLocalMatrix = true;

#if ENGINE_EDITOR
    this->eulerAngles = glm::eulerAngles(rotation);
#endif

    TransformChanged();
}

void GameObject::SetRotation(const glm::quat& rotation)
{
    glm::quat rot = rotation;

    if (parent != nullptr)
    {
        glm::quat p = parent->GetRotation();
        p.x = -p.x;
        p.y = -p.y;
        p.z = -p.z;
        rot = p * rotation;
    }

    SetLocalRotation(rot);
}

void GameObject::SetLocalPosition(const float3& localPosition)
{
    if (position == localPosition)
    {
        return;
    }
    this->position = localPosition;
    this->updateLocalMatrix = true;

    TransformChanged();
}

void GameObject::SetLocalScale(const float3& scale)
{
    if (this->scale == scale)
    {
        return;
    }

    this->scale = scale;
    updateLocalMatrix = true;
    TransformChanged();
}

// void GameObject::ResetAsPrototype()
// {
//     // sanity check, there shouldn't have any owningChildren
//     if (!owningChildren.empty())
//         return;
//
//     if (prototype == nullptr)
//         return;
//
//     Scene* scene = GetScene();
//     if (scene == nullptr)
//         return;
//
//     for (auto child : children)
//     {
//         scene->DestroyGameObject(child);
//     }
//     children.clear();
//
//     SetEnable(false);
//     components.clear();
//
//     for (auto& c : prototype->components)
//     {
//         components.push_back(c->Clone(*this));
//     }
//
//     for (GameObject* child : prototype->children)
//     {
//         std::unique_ptr<GameObject> newChild = std::make_unique<GameObject>(*child);
//         GameObject* tmp = newChild.get();
//         scene->AddGameObject(std::move(newChild));
//         tmp->SetParent(this);
//     }
//
//     if (wantsToBeEnabled)
//     {
//         for (auto& c : components)
//         {
//             if (c->IsEnabled())
//             {
//                 c->OnEnable();
//             }
//         }
//         wantsToBeEnabled = false;
//     }
// }

GameObject* GameObject::Find(std::string_view name)
{
    if (this->name == name)
    {
        return this;
    }

    for (auto c : children)
    {
        if (auto found = c->Find(name))
        {
            return found;
        }
    }

    return nullptr;
}

void GameObject::LinkPrefab(Prefab* prefab)
{
    this->prefab = prefab;
}

void GameObject::ApplyToPrefab()
{
    if (prefab)
    {
        prefab->SetGameObject(this);
    }
}

void GameObject::ResetToPrefab()
{
    auto prefabInstance = prefab->GetGameObject();
    auto scene = GetScene();
    auto parent = GetParent();
    auto worldMatrix = GetWorldMatrix();

    if (prefabInstance && scene)
    {
        scene->DestroyGameObject(this);
        auto newGo = scene->AddGameObject(std::make_unique<GameObject>(*prefabInstance));
        newGo->SetParent(parent);
        newGo->SetWorldMatrix(worldMatrix);
    }
}

ObjPtr<Component> GameObject::GetComponentInHierarchy(const char* className)
{
    Component* found = GetComponent(className);

    if (found)
        return found;

    for (auto& c : children)
    {
        found = c->GetComponentInHierarchy(className);
        if (found)
            return found;
    }

    return nullptr;
}

ObjPtr<Component> GameObject::GetComponent(const char* className)
{
    for (auto& c : allComponents)
    {
        if (c && c->GetTypeName().compare(className) == 0)
        {
            return c;
        }
    }

    return nullptr;
}

int GameObject::RegisterContactEventAdded(
    const std::function<void(PhysicsBody*, PhysicsBody*, const JPH::ContactManifold&, JPH::ContactSettings&)>& f
)
{
    for (int i = 0; i < contactAddedCallbacks.size(); ++i)
    {
        if (contactAddedCallbacks[i] == nullptr)
        {
            contactAddedCallbacks[i] = f;
            return i;
        }
    }

    contactAddedCallbacks.push_back(f);
    return contactAddedCallbacks.size() - 1;
}

int GameObject::RegisterContactEventRemoved(
    const std::function<void(PhysicsBody*, PhysicsBody*, const JPH::ContactManifold&, JPH::ContactSettings&)>& f
)
{

    for (int i = 0; i < contactRemovedCallbacks.size(); ++i)
    {
        if (contactRemovedCallbacks[i] == nullptr)
        {
            contactRemovedCallbacks[i] = f;
            return i;
        }
    }

    contactRemovedCallbacks.push_back(f);
    return contactRemovedCallbacks.size() - 1;
}

Component* GameObject::AddComponent(std::string_view componentName)
{
    auto p = ObjectRegistry::CreateObjectByName(componentName);
    Component* temp = dynamic_cast<Component*>(p.release());
    if (temp == nullptr)
        return nullptr;

    temp->gameObject = this;
    std::unique_ptr<Component> compPtr(temp);
    components.push_back(std::move(compPtr));
    temp->OnInit();
    temp->Enable();

    UpdateAllComponents();
    return temp;
}

void GameObject::TransformChanged()
{
    transformChanged = true;

    for (auto& c : components)
        c->TransformChanged();

    for (auto child : children)
    {
        child->TransformChanged();
    }
}

void GameObject::RemoveComponent(void* comp)
{
    auto iter = std::find_if(components.begin(), components.end(), [comp](auto& p)
                             { return p.get() == comp; });
    if (iter != components.end())
    {
        std::unique_ptr<Component>& comp = *iter;
        comp->Disable();
        components.erase(iter);
    }

    UpdateAllComponents();
}

void GameObject::RemoveComponentByIndex(int componentIndex)
{
    if (componentIndex >= 0 && componentIndex < components.size())
    {
        auto& comp = components[componentIndex];
        if (comp != nullptr)
        {
            comp->Disable();
        }
        components.erase(components.begin() + componentIndex);
    }

    UpdateAllComponents();
}

bool GameObject::GetWantsTobeEnabledStateAndReset()
{
    bool temp = wantsToBeEnabled;
    wantsToBeEnabled = false;
    return temp;
}

void GameObject::UnregisterContactEventAdded(int id)
{
    if (id >= 0 && id < contactAddedCallbacks.size())
        contactAddedCallbacks[id] = nullptr;
}

void GameObject::UnregisterContactEventRemoved(int id)
{
    if (id >= 0 && id < contactRemovedCallbacks.size())
        contactRemovedCallbacks[id] = nullptr;
}

void GameObject::OnContactAdded(
    PhysicsBody* body1, PhysicsBody* body2, const JPH::ContactManifold& manifold, JPH::ContactSettings& settings
)
{
    for (auto& f : contactAddedCallbacks)
    {
        if (f)
            f(body1, body2, manifold, settings);
    }
}

void GameObject::OnContactRemoved(
    PhysicsBody* body1, PhysicsBody* body2, const JPH::ContactManifold& manifold, JPH::ContactSettings& settings
)
{
    for (auto& f : contactRemovedCallbacks)
    {
        if (f)
            f(body1, body2, manifold, settings);
    }
}

void GameObject::OnAwake()
{
    for (auto& c : components)
    {
        if (c->IsEnabled())
            c->OnAwake();
    }
}

void GameObject::OnStart()
{
    for (auto& c : components)
    {
        c->OnStart();
    }
}

void GameObject::OnStop()
{
    for (auto& c : components)
    {
        c->OnStop();
    }
}

float3 GameObject::GetPosition() const
{
    return GetWorldMatrix()[3];
}

float3 GameObject::GetScale() const
{
    auto m = GetWorldMatrix();
    return {glm::length(float3(m[0])), glm::length(float3(m[1])), glm::length(float3(m[2]))};
}

float3 GameObject::GetForward() const
{
    return glm::normalize(glm::vec3(glm::mat4_cast(GetRotation())[2]));
}

float3 GameObject::GetUp() const
{
    return glm::normalize(glm::vec3(glm::mat4_cast(GetRotation())[1]));
}

float3 GameObject::GetRight() const
{
    return glm::normalize(glm::vec3(glm::mat4_cast(GetRotation())[0]));
}

void GameObject::SetEulerAngles(const float3& eulerAngles)
{
    if (this->eulerAngles == eulerAngles)
        return;

    this->eulerAngles = eulerAngles;
    auto rotation = glm::quat(eulerAngles);

    // set local rotation
    this->rotation = rotation;
    updateLocalMatrix = true;

    TransformChanged();
}

void GameObject::Rotate(const float3& axis, float angle, RotationCoordinate coord)
{
    if (angle == 0)
        return;

    updateLocalMatrix = true;
    if (coord == RotationCoordinate::Self)
    {
        rotation = glm::rotate(rotation, angle, axis);
    }
    else if (coord == RotationCoordinate::Parent && parent != nullptr)
    {
        // TODO: implement parent rotation
    }
    else if (coord == RotationCoordinate::World)
    {
        // rotate around world
        float4x4 trs = glm::rotate(glm::mat4(1), angle, axis) * GetWorldMatrix();
        SetWorldMatrix(trs);
    }

    TransformChanged();
}

void GameObject::Rotate(glm::quat quaternion)
{
    SetLocalRotation(quaternion * rotation);
}

void GameObject::RotateAround(const float3& point, const float3& axis, float angle)
{
    float4x4 trs = glm::translate(float4x4(1), point) * glm::rotate(float4x4(1), angle, axis) *
                   glm::translate(float4x4(1), -point) * GetWorldMatrix();
    SetWorldMatrix(trs);
}

void GameObject::LookAt(const float3& to)
{
    if (glm::length(to) < compareEpsilon)
        return;

    float3 forward = glm::normalize(to);
    float3 up = float3(0, 1, 0);

    // Handle case where forward is parallel to up vector
    if (glm::abs(glm::dot(forward, up)) > 0.99f)
    {
        up = float3(1, 0, 0);
    }

    float3 right = glm::normalize(glm::cross(forward, up));
    up = glm::cross(right, forward);

    float3x3 rotationMatrix = float3x3(right, up, -forward);
    glm::quat newRotation = glm::quat_cast(rotationMatrix);

    SetRotation(newRotation);
}

void GameObject::Translate(const float3& translate)
{
    if (translate == float3{0, 0, 0})
        return;

    updateLocalMatrix = true;
    this->position += translate;

    for (GameObject* child : children)
    {
        child->Translate(translate);
    }

    TransformChanged();
}

void GameObject::MoveInComponent(Component* otherPtr)
{
    auto otherGO = otherPtr->GetGameObject();

    if (otherGO == nullptr)
        return;

    auto iter = std::find_if(otherGO->components.begin(), otherGO->components.end(), [otherPtr](auto& p)
                             { return p.get() == otherPtr; });

    if (iter != otherGO->components.end())
    {
        bool isInEnableState = !otherPtr->IsEnabled();
        if (isInEnableState)
            otherPtr->Disable();

        std::unique_ptr<Component> owned = std::move(*iter);
        otherGO->components.erase(iter);

        components.push_back(std::move(owned));

        if (isInEnableState)
            otherPtr->Enable();
    }

    UpdateAllComponents();
}

void GameObject::UpdateAllComponents()
{
    allComponents.clear();

    for (auto& c : components)
    {
        allComponents.push_back(c.get());
    }

    for (auto& c : prefabComponents)
    {
        allComponents.push_back(c.get());
    }
}

void GameObject::ApplyPrefabComponents()
{
    if (prefab != nullptr)
    {
        prefabComponents.clear();

        auto comps = prefab->GetGameObject()->GetComponents();
        for (auto comp : comps)
        {
            prefabComponents.push_back(comp->Clone(*this));
        }
    }
}

int GameObject::LuaGetComponent(lua_State* L)
{
    GameObject* go = GetLuaUserDataPackValue<GameObject>(L, 1);
    const char* className = luaL_checkstring(L, 2);

    if (go == nullptr || className == nullptr)
        return 0;

    auto v = go->GetComponent(className);
    if (v != nullptr)
    {
        LuaBinder<GameObject>::ProcessRtn(L, std::move(v));
        return 1;
    }

    // failed to get Engine Component, try Lua Script
    auto gameScript = go->GetComponent<GameScript>();
    if (gameScript != nullptr)
    {
        auto& luaClassName = gameScript->GetLuaClassName();
        if (luaClassName == className)
        {
            return gameScript->LuaPushReferenceToStack();
        }
    }

    return 0;
}
