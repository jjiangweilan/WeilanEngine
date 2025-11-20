#include "GameObject.hpp"
#include "Core/Component/GameScript.hpp"
#include "Core/Prefab.hpp"
#include "Core/Scene/Scene.hpp"
#include "Libs/Math.hpp"
#include <glm/gtx/matrix_decompose.hpp>
#include <spdlog/spdlog.h>
DEFINE_OBJECT(GameObject, "F04CAB0A-DCF0-4ECF-A690-13FBD63A1AC7");

GameObject::GameObject() : gameScene(nullptr)
{
    name = "New GameObject";
    ResetTransform();
}

GameObject::GameObject(GameObject&& other)
    : components(std::move(other.components)), gameScene(std::exchange(other.gameScene, nullptr))
{
    SetName(other.GetName());
    ResetTransform();
}

GameObject::GameObject(Scene* gameScene) : gameScene(gameScene)
{
    name = "New GameObject";
    ResetTransform();
}

void GameObject::ResetTransform()
{
    position = glm::vec3(0);
    scale = glm::vec3(1);
    rotation = glm::identity<glm::quat>();
    eulerAngles = glm::vec3(0, 0, 0);
}

void GameObject::Copy(const GameObject& other)
{
    SetName(other.GetName());
    position = other.position;
    scale = other.scale;
    rotation = other.rotation;
    eulerAngles = other.eulerAngles;
    wantsToBeEnabled = other.enabled;
    gameScene = nullptr;
    enabled = false;

    for (auto& c : other.components)
    {
        components.push_back(c->Clone(*this));
    }

    for (GameObject* child : other.children)
    {
        owningChildren.push_back(std::make_unique<GameObject>(*child));
        owningChildren.back()->SetParent(this, false);
    }
}

GameObject::GameObject(const GameObject& other)
{
    Copy(other);
}

GameObject::~GameObject()
{
    components.clear();
}

void GameObject::Tick()
{
    for (auto& comp : components)
    {
        if (comp && comp->IsEnabled())
            comp->Tick();
    }
}

void GameObject::IdleTick()
{
    for (auto& comp : components)
    {
        if (comp && comp->IsEnabled())
            comp->IdleTick();
    }
}

void GameObject::PrePhysicsTick()
{
    for (auto& comp : components)
    {
        if (comp && comp->IsEnabled())
            comp->PrePhysicsTick();
    }
}

std::vector<std::unique_ptr<Component>>& GameObject::GetComponents()
{
    return components;
}

Scene* GameObject::GetScene()
{
    return gameScene;
}

void GameObject::Serialize(Serializer* s) const
{
    Object::Serialize(s);
    s->Serialize("components", components);
    s->Serialize("rotation", rotation);
    s->Serialize("position", position);
    s->Serialize("scale", scale);
    s->Serialize("parent", parent);
    s->Serialize("children", children);
    s->Serialize("enabled", enabled);
    s->Serialize("prefab", prefab);
}

void GameObject::SetWorldMatrix(const glm::mat4& matrix)
{
    auto localMatrix = matrix;
    if (parent)
        localMatrix = glm::inverse(parent->GetWorldMatrix()) * matrix;

    glm::vec3 position{};
    glm::vec3 scale{};
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
    eulerAngles = glm::eulerAngles(rotation);
    s->Deserialize("components", components);
    s->Deserialize("prefab", prefab);
    // gameScene is set by Scene when it's deserializing
}

void GameObject::OnLoaded()
{
    for (auto& c : components)
    {
        if (c)
            c->gameObject = this;
    }

    for (auto& c : components)
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
        glm::mat4 parentWorld = glm::mat4(1);
        if (newParent != nullptr)
        {
            parentWorld = newParent->GetWorldMatrix();
        }
        glm::mat4 currentWorld = GetWorldMatrix();

        glm::mat4 local = glm::inverse(parentWorld) * currentWorld;

        glm::vec3 newPosition, newScale;
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
            for (auto& c : components)
            {
                if (c && c->IsEnabled())
                    c->OnDisable();
            }
        }

        this->gameScene = scene;
        for (auto& c : components)
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
    if (enabled == isEnabled || gameScene == nullptr)
        return;

    for (auto child : children)
    {
        // child may be nullptr when deserializing
        if (child)
        {
            child->SetEnable(isEnabled);
        }
    }

    if (isEnabled)
    {
        for (auto& c : components)
        {
            if (c && c->IsEnabled())
            {
                c->OnEnable();
            }
        }
    }
    else
    {
        for (auto& c : components)
        {
            if (c && c->IsEnabled())
                c->OnDisable();
        }
    }

    enabled = isEnabled;
}

void GameObject::SetScale(const glm::vec3& s)
{
    SetLocalScale(s);
}

const glm::mat4& GameObject::GetLocalMatrix() const
{
    if (updateLocalMatrix)
    {
        localMatrix =
            glm::translate(glm::mat4(1), position) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1), scale);
        updateLocalMatrix = false;
    }

    return localMatrix;
}

glm::mat4 GameObject::GetWorldMatrix() const
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

void GameObject::SetPosition(const glm::vec3& position)
{
    glm::vec3 pos = position;
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

void GameObject::SetLocalPosition(const glm::vec3& localPosition)
{
    if (position == localPosition)
    {
        return;
    }
    this->position = localPosition;
    this->updateLocalMatrix = true;

    TransformChanged();
}

void GameObject::SetLocalScale(const glm::vec3& scale)
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

ObjPtr<Component> GameObject::GetComponentInHierachy(const char* className)
{
    Component* found = GetComponent(className);

    if (found)
        return found;

    for (auto& c : children)
    {
        found = c->GetComponentInHierachy(className);
        if (found)
            return found;
    }

    return nullptr;
}

ObjPtr<Component> GameObject::GetComponent(const char* className)
{
    for (auto& c : components)
    {
        if (c && c->GetTypeName().compare(className) == 0)
        {
            return c.get();
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

glm::vec3 GameObject::GetPosition() const
{
    return GetWorldMatrix()[3];
}

glm::vec3 GameObject::GetScale() const
{
    auto m = GetWorldMatrix();
    return {glm::length(glm::vec3(m[0])), glm::length(glm::vec3(m[1])), glm::length(glm::vec3(m[2]))};
}

glm::vec3 GameObject::GetForward() const
{
    return glm::normalize(glm::vec3(glm::mat4_cast(GetRotation())[2]));
}

glm::vec3 GameObject::GetUp() const
{
    return glm::normalize(glm::vec3(glm::mat4_cast(GetRotation())[1]));
}

glm::vec3 GameObject::GetRight() const
{
    return glm::normalize(glm::vec3(glm::mat4_cast(GetRotation())[0]));
}

void GameObject::SetEulerAngles(const glm::vec3& eulerAngles)
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

void GameObject::Rotate(const glm::vec3& axis, float angle, RotationCoordinate coord)
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
        glm::mat4 trs = glm::rotate(glm::mat4(1), angle, axis) * GetWorldMatrix();
        SetWorldMatrix(trs);
    }

    TransformChanged();
}

void GameObject::Rotate(glm::quat quaternion)
{
    SetLocalRotation(quaternion * rotation);
}

void GameObject::RotateAround(const glm::vec3& point, const glm::vec3& axis, float angle)
{
    glm::mat4 trs = glm::translate(glm::mat4(1), point) * glm::rotate(glm::mat4(1), angle, axis) *
                    glm::translate(glm::mat4(1), -point) * GetWorldMatrix();
    SetWorldMatrix(trs);
}

void GameObject::LookAt(const glm::vec3& to)
{
    if (glm::length(to) < compareEpsilon)
        return;

    glm::vec3 forward = glm::normalize(to);
    glm::vec3 up = glm::vec3(0, 1, 0);

    // Handle case where forward is parallel to up vector
    if (glm::abs(glm::dot(forward, up)) > 0.99f)
    {
        up = glm::vec3(1, 0, 0);
    }

    glm::vec3 right = glm::normalize(glm::cross(forward, up));
    up = glm::cross(right, forward);

    glm::mat3 rotationMatrix = glm::mat3(right, up, -forward);
    glm::quat newRotation = glm::quat_cast(rotationMatrix);

    SetRotation(newRotation);
}

void GameObject::Translate(const glm::vec3& translate)
{
    if (translate == glm::vec3{0, 0, 0})
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
}
