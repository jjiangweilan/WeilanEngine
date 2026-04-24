#pragma once
#include "Engine/Core/Asset.hpp"
#include <memory>
class GameObject;
class [[LuaClass]] Prefab : public Asset
{
    DECLARE_ASSET()

public:
    Prefab(GameObject* gameObject = nullptr);

    std::unique_ptr<GameObject> Instantiate();

    GameObject* GetGameObject() const { return gameObject.get(); }
    void SetGameObject(GameObject* gameObject);

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;

    void OnLoaded() override;
private:
    std::unique_ptr<GameObject> gameObject;
};
