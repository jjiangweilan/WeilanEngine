#include "Component.hpp"
#include "Engine/Runtime/System/SceneManager/Scene.hpp"
#include "Engine/Core/GameLoop.hpp"
#include "Engine/Runtime/Object/GameObject/GameObject.hpp"

DEFINE_OBJECT(Object, Component, "2C3B2BF0-8BBD-4373-AC09-5B7AD2128630")

Component::Component(GameObject* gameObject) : gameObject(gameObject) {}

Component::~Component()
{}

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
        if (gameObject->IsActiveInScene())
        {
            Awake();
            if (GameLoop::IsPlaying())
                Start();
            OnEnable();
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
    return gameObject && gameObject->IsActiveInScene() && enabled;
}
