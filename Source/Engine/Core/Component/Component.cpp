#include "Component.hpp"
#include "Core/GameObject.hpp"

namespace Engine
{
Component::Component(std::string_view name, RefPtr<GameObject> gameObject) : gameObject(gameObject.Get()), name(name)
{
    this->gameObject = gameObject.Get();
}

Component::~Component() {}

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
