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

void Component::Enable()
{
    if (enabled == false)
    {
        enabled = true;
        if (gameObject->IsEnabled())
            OnEnable();

        if (!isAwake)
        {
            isAwake = true;
            OnAwake();
        }
    }
}

void Component::Disable()
{
    if (enabled == true)
    {
        enabled = false;
        if (gameObject->IsEnabled())
            OnDisable();
    }
}

bool Component::IsActiveInScene()
{
    return gameObject && gameObject->IsEnabled() && enabled;
}
