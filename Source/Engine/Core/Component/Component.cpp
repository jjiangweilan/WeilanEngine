#include "Component.hpp"
#include "Core/GameObject.hpp"

namespace Engine
{
Component::Component(std::string_view name, RefPtr<GameObject> gameObject) : name(name), gameObject(gameObject.Get())
{
    this->gameObject = gameObject.Get();
}

GameObject* Component::GetGameObject()
{
    return gameObject;
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
