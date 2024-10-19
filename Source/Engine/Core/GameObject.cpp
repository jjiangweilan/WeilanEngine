#include "GameObject.hpp"
#include "Core/Scene/Scene.hpp"
#include "Libs/Math.hpp"
#include <glm/gtx/matrix_decompose.hpp>
DEFINE_ASSET(GameObject, "F04CAB0A-DCF0-4ECF-A690-13FBD63A1AC7", "gobj");

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

std::unique_ptr<Asset> GameObject::Clone()
{
    auto clone = std::make_unique<GameObject>(*this);
    return clone;
}

GameObject::GameObject(const GameObject& other)
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
        owningChildren.back()->SetParent(this);
    }
}

GameObject::~GameObject()
{
    components.clear();
}

void GameObject::Tick()
{
    for (auto& comp : components)
    {
        comp->Tick();
    }
}

void GameObject::PrePhysicsTick()
{
    for (auto& comp : components)
    {
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
    Asset::Serialize(s);
    s->Serialize("components", components);
    s->Serialize("rotation", rotation);
    s->Serialize("position", position);
    s->Serialize("scale", scale);
    s->Serialize("parent", parent);
    s->Serialize("children", children);
    s->Serialize("enabled", enabled);
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
    SetScale(scale);
    SetLocalPosition(position);
}

void GameObject::Deserialize(Serializer* s)
{
    Asset::Deserialize(s);
    s->Deserialize("enabled", enabled);
    s->Deserialize("children", children);
    s->Deserialize("parent", parent);
    s->Deserialize("scale", scale);
    s->Deserialize("position", position);
    s->Deserialize("rotation", rotation);
    eulerAngles = glm::eulerAngles(rotation);
    s->Deserialize("components", components);
    // gameScene is set by Scene when it's deserializing
}

void GameObject::OnLoadingFinished()
{
    for (auto& c : components)
    {
        c->gameObject = this;
    }
}

void GameObject::RemoveChild(GameObject* child)
{
    auto it = children.begin();
    while (it != children.end())
    {
        if (*it == child)
        {
            children.erase(it);
            return;
        }
        it += 1;
    }
}

void GameObject::SetParent(GameObject* parent)
{
    if (this->parent == parent)
        return;

    if (parent == nullptr)
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
    glm::mat4 parentWorld = glm::mat4(1);
    if (parent != nullptr)
    {
        parentWorld = parent->GetWorldMatrix();
    }
    glm::mat4 currentWorld = GetWorldMatrix();
    glm::mat4 local = glm::inverse(parentWorld) * currentWorld;

    glm::vec3 position, scale;
    glm::quat rotation;
    Math::DecomposeMatrix(local, position, scale, rotation);
    SetLocalPosition(position);
    SetLocalRotation(rotation);
    SetLocalScale(scale);

    this->parent = parent;
    if (parent)
        parent->children.push_back(this);
}

void GameObject::SetScene(Scene* scene)
{
    if (this->gameScene != scene)
    {
        if (this->gameScene != nullptr)
        {
            for (auto& c : components)
            {
                if (c->IsEnabled())
                    c->DisableImple();
            }
        }

        this->gameScene = scene;
        for (auto& c : components)
        {
            if (c->IsEnabled() && enabled)
                c->EnableImple();
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
                c->EnableImple();
            }
        }
    }
    else
    {
        for (auto& c : components)
        {
            if (c->IsEnabled())
                c->DisableImple();
        }
    }

    enabled = isEnabled;
}

void GameObject::SetScale(const glm::vec3& s)
{
    SetLocalScale(s);
}

glm::mat4 GameObject::GetWorldMatrix() const
{
    if (updateLocalMatrix)
    {
        localMatrix =
            glm::translate(glm::mat4(1), position) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1), scale);
        updateLocalMatrix = false;
    }

    auto finalMatrix = localMatrix;
    if (parent != nullptr)
    {
        finalMatrix = parent->GetWorldMatrix() * localMatrix;
    }

    return finalMatrix;
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
    if (this->scale != scale)
    {
        this->scale = scale;
        updateLocalMatrix = true;
        TransformChanged();
    }
}
