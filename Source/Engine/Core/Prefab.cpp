#include "Prefab.hpp"
#include "Core/GameObject.hpp"

DEFINE_ASSET(Prefab, "E7A241A8-D2F2-494C-BEB9-4934B4D2C2F9", "prefab")

Prefab::Prefab(GameObject* gameObject)
{
    if (gameObject)
    {
        this->gameObject = std::make_unique<GameObject>(*gameObject);
        this->gameObject->LinkPrefab(this);
    }
}

std::unique_ptr<GameObject> Prefab::Instantiate()
{
    auto go = std::make_unique<GameObject>(*gameObject);
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
