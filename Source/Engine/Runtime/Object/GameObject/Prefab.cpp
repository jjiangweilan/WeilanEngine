#include "Prefab.hpp"
#include "Engine/Library/TypeReflection.hpp"
#include "Engine/Runtime/Object/GameObject/GameObject.hpp"

DEFINE_ASSET(Prefab, "E7A241A8-D2F2-494C-BEB9-4934B4D2C2F9", "prefab")

TYPE_REFLECTION_MEMBER_VARIABLES(
    Prefab,
    TYPE_REFLECTION_MEM(Prefab, gameObject)
);

Prefab::Prefab(GameObject* gameObject)
{
    if (gameObject)
    {
        this->gameObject = std::make_unique<GameObject>();
        this->gameObject->Copy(*gameObject, GameObject::ComponentCopyMode::EffectiveComponentsAsLocal);
        this->gameObject->LinkPrefab(this);
        SetDirty(true);
    }
}

void Prefab::SetGameObject(GameObject* gameObject)
{
    if (gameObject)
    {
        this->gameObject->Copy(*gameObject, GameObject::ComponentCopyMode::EffectiveComponentsAsLocal);
        this->gameObject->LinkPrefab(this);

        SetDirty(true);
    }
}

std::unique_ptr<GameObject> Prefab::Instantiate()
{
    auto go = std::make_unique<GameObject>();
    go->Copy(*gameObject, GameObject::ComponentCopyMode::EffectiveComponentsAsPrefab);
    go->LinkPrefab(this);
    return std::move(go);
}

void Prefab::Serialize(Serializer* s) const
{
    Asset::Serialize(s);
    s->Serialize("gameObject", gameObject);
}
void Prefab::Deserialize(Serializer* s)
{  

    Asset::Deserialize(s);
    s->Deserialize("gameObject", gameObject);
}

void Prefab::OnLoaded()
{
    if (gameObject)
    {
        gameObject->OnLoaded();
    }
}
