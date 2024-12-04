#include "Component.hpp"
#include "Core/GameObject.hpp"

Component::Component(GameObject* gameObject) : gameObject(gameObject) {}

Component::~Component()
{
    OnDestroy();
}

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
    Object::Serialize(s);
    s->Serialize("gameObject", gameObject);
    s->Serialize("enabled", enabled);
}

void Component::Deserialize(Serializer* s)
{
    Object::Deserialize(s);
    s->Deserialize("gameObject", gameObject);
    s->Deserialize("enabled", enabled);
}
