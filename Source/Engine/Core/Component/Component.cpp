#include "Component.hpp"
#include "Core/GameObject.hpp"

namespace Engine
{
Component::Component(GameObject* gameObject) : gameObject(gameObject) {}

Component::~Component() {}

GameObject* Component::GetGameObject()
{
    return gameObject;
}

Scene* Component::GetScene()
{
    if (gameObject)
        return gameObject->GetScene();
    return nullptr;
}

void Component::Serialize(Serializer* s) const
{
    s->Serialize("uuid", uuid);
    s->Serialize("gameObject", gameObject);
}

void Component::Deserialize(Serializer* s)
{
    s->Deserialize("uuid", uuid);
    s->Deserialize("gameObject", gameObject);
}
} // namespace Engine
