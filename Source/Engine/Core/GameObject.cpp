#include "GameObject.hpp"
#include "Component/Transform.hpp"
#include <stdexcept>

namespace Engine
{
GameObject::GameObject() : gameScene(nullptr)
{
    transform = AddComponent<Transform>();
    name = "New GameObject";

    SERIALIZE_MEMBER(components);
}
GameObject::GameObject(GameObject&& other)
    : components(std::move(other.components)), transform(std::exchange(other.transform, nullptr)),
      gameScene(std::exchange(other.gameScene, nullptr))
{
    other.name = std::move(name);
    SERIALIZE_MEMBER(components);
}

GameObject::GameObject(RefPtr<GameScene> gameScene) : gameScene(gameScene)
{
    transform = AddComponent<Transform>();
    name = "New GameObject";
    SERIALIZE_MEMBER(components);
}

GameObject::~GameObject() {}

RefPtr<Transform> GameObject::GetTransform() { return transform; }

void GameObject::Tick()
{
    for (auto& comp : components)
    {
        comp->Tick();
    }
}

std::vector<UniPtr<Component>>& GameObject::GetComponents() { return components; }

RefPtr<GameScene> GameObject::GetGameScene() { return gameScene; }

void GameObject::DeserializeInternal(const std::string& nameChain,
                                     AssetSerializer& serializer,
                                     ReferenceResolver& refResolver)
{
    AssetObject::DeserializeInternal(nameChain, serializer, refResolver);

    transform = GetComponent<Transform>();
    for (auto& c : components)
    {
        c->gameObject = this;
    }
}

RefPtr<Component> GameObject::GetComponent(const char* name)
{
    for (auto& c : components)
    {
        if (strcmp(c->GetName().c_str(), name) == 0)
        {
            return c;
        }
    }

    return nullptr;
}
} // namespace Engine
