#pragma once
#include "Core/Asset.hpp"

class GameObject;
class Prefab : public Asset
{
    DECLARE_ASSET()

public:
    Prefab(GameObject* gameObject = nullptr);

    std::unique_ptr<GameObject> Instantiate();

    const GameObject* GetGameObject() const { return gameObject.get(); }

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;

private:
    std::unique_ptr<GameObject> gameObject;
};
