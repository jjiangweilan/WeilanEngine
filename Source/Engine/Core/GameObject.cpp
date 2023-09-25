#include "GameObject.hpp"
#include "Component/Transform.hpp"
#include "Core/Scene/Scene.hpp"
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
    other.name = std::move(name);
}

GameObject::GameObject(RefPtr<Scene> gameScene) : gameScene(gameScene)
{
    transform = AddComponent<Transform>();
    name = "New GameObject";
}

GameObject::~GameObject() {}

Transform* GameObject::GetTransform()
{
    return transform.Get();
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

RefPtr<Scene> GameObject::GetGameScene()
{
    return gameScene;
}

RefPtr<Component> GameObject::GetComponent(const char* name)
{
    for (auto& c : components)
    {
        if (c->GetName() == name)
        {
            return c;
        }
    }

    return nullptr;
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
