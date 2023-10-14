#include "GameObject.hpp"
#include "Component/Transform.hpp"
#include "Core/Scene/Scene.hpp"
#include <algorithm>
#include <stdexcept>

namespace Engine
{
DEFINE_ASSET(GameObject, "F04CAB0A-DCF0-4ECF-A690-13FBD63A1AC7", "gobj");

GameObject::GameObject() : gameScene(nullptr)
{
    transform = AddComponent<Transform>();
    name = "New GameObject";
}

GameObject::GameObject(GameObject&& other)
    : components(std::move(other.components)), transform(std::exchange(other.transform, nullptr)),
      gameScene(std::exchange(other.gameScene, nullptr))
{
    SetName(other.GetName());
}

GameObject::GameObject(Scene* gameScene) : gameScene(gameScene)
{
    transform = AddComponent<Transform>();
    name = "New GameObject";
}

GameObject::GameObject(const GameObject& other)
{
    SetName(other.GetName());

    for (auto& c : other.components)
    {
        components.push_back(c->Clone(*this));
    }

    auto tsmIter = std::find_if(
        components.begin(),
        components.end(),
        [](const std::unique_ptr<Component>& c)
        {
            Component& dc = *c;
            return typeid(dc) == typeid(Transform);
        }
    );

    assert(tsmIter != components.end());
    transform = (Transform*)tsmIter->get();
}

GameObject::~GameObject() {}

Transform* GameObject::GetTransform()
{
    return transform;
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

Scene* GameObject::GetGameScene()
{
    return gameScene;
}

void GameObject::Serialize(Serializer* s) const
{
    Asset::Serialize(s);
    s->Serialize("components", components);
    s->Serialize("transform", transform);
    s->Serialize("gameScene", gameScene);
}

void GameObject::Deserialize(Serializer* s)
{
    Asset::Deserialize(s);
    s->Deserialize("components", components);
    s->Deserialize("transform", transform);
    s->Deserialize("gameScene", gameScene);
}

} // namespace Engine
