#include "Component.hpp"
#include "Core/GameObject.hpp"

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
    s->Serialize("enabled", enabled);
}

void Component::Deserialize(Serializer* s)
{
    s->Deserialize("uuid", uuid);
    s->Deserialize("gameObject", gameObject);
    s->Deserialize("enabled", enabled);
}
