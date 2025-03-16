#include "GameObject.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
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
        if (comp->IsEnabled())
            comp->Tick();
    }
}

void GameObject::IdleTick()
{
    for (auto& comp : components)
    {
        if (comp->IsEnabled())
            comp->IdleTick();
    }
}

void GameObject::PrePhysicsTick()
{
    for (auto& comp : components)
    {
        if (comp->IsEnabled())
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
        c->gameObject = this;
    }

    for (auto& c : components)
    {
        c->OnLoaded();
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
                if (c->IsEnabled())
                    c->OnDisable();
            }
        }

        this->gameScene = scene;
        for (auto& c : components)
        {
            if (c->IsEnabled() && enabled)
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
            if (c->IsEnabled())
            {
                c->OnEnable();
            }
        }
    }
    else
    {
        for (auto& c : components)
        {
            if (c->IsEnabled())
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
        if (c->GetTypeName().compare(className) == 0)
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
    temp->Enable();
    return temp;
}

void GameObject::LookAt(const float3& lookAtPos)
{
    auto pos = GetPosition();
    auto mat = glm::lookAt(pos, lookAtPos, {0, 1, 0});
    float3x3 rotMat = mat;
    rotMat = glm::transpose(rotMat);
    auto rot = glm::quat_cast(rotMat);
    SetRotation(rot);
}
