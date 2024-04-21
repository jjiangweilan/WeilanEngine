#include "GameObject.hpp"
#include "Core/Scene/Scene.hpp"
#include <algorithm>
#include <glm/gtx/matrix_decompose.hpp>
#include <stdexcept>

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
}

GameObject::GameObject(const GameObject& other)
{
    SetName(other.GetName());
    position = other.position;
    scale = other.scale;
    rotation = other.rotation;
    gameScene = other.gameScene;
    enabled = other.enabled;

    for (auto& c : other.components)
    {
        components.push_back(c->Clone(*this));
    }

    if (gameScene)
    {
        for (GameObject* child : other.children)
        {
            GameObject* childClone = gameScene->AddGameObject(std::make_unique<GameObject>(*child));
            childClone->SetParent(this);
        }
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
    s->Serialize("gameScene", gameScene);
    s->Serialize("rotation", rotation);
    s->Serialize("position", position);
    s->Serialize("scale", scale);
    s->Serialize("parent", parent);
    s->Serialize("children", children);
    s->Serialize("enabled", enabled);
}

void GameObject::SetModelMatrix(const glm::mat4& model)
{
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::vec3 position;
    glm::decompose(model, scale, rotation, position, skew, perspective);

    // this is needed to propagate translation to children
    SetPosition(position);
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
    s->Deserialize("gameScene", gameScene);
    s->Deserialize("components", components);

    for (auto& c : components)
    {
        c->gameObject = this;
    }

    if (enabled)
    {
        for (auto& c : components)
        {
            if (c->IsEnabled())
                c->EnableImple();
        }
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
        this->parent = nullptr;
    }
    else if (this->parent == nullptr)
    {
        Scene* scene = GetScene();
        if (scene)
            scene->RemoveGameObjectFromRoot(this);
        this->parent = parent;
        parent->children.push_back(this);
    }
    else
    {
        this->parent->RemoveChild(this);
        this->parent = parent;
        parent->children.push_back(this);
    }
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
    }
}

void GameObject::SetEnable(bool isEnabled)
{
    if (enabled == isEnabled)
        return;

    for (auto child : children)
    {
        // child may be nullptr when deserializing
        if (child)
            child->SetEnable(isEnabled);
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
