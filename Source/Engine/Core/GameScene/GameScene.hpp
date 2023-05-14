#pragma once

#include "Core/Component/Light.hpp"
#include "Core/GameObject.hpp"
#include "Core/Resource.hpp"
namespace Engine
{
class GameScene : public Resource
{
public:
    GameScene();
    ~GameScene() {}
    RefPtr<GameObject> CreateGameObject();
    void AddGameObject(GameObject* newGameObject);

    const std::vector<RefPtr<GameObject>>& GetRootObjects();

    void Tick();

    void MoveGameObjectToRoot(RefPtr<GameObject> obj);
    void RemoveGameObjectFromRoot(RefPtr<GameObject> obj);
    void RemoveGameObject(GameObject* obj);

    std::vector<RefPtr<Light>> GetActiveLights();

    void Serialize(Serializer* s) override;
    void Deserialize(Serializer* s) override;

protected:
    std::vector<UniPtr<GameObject>> gameObjects;
    std::vector<RefPtr<GameObject>> externalGameObjects;
    std::vector<RefPtr<GameObject>> roots;

    void TickGameObject(RefPtr<GameObject> obj);
};
} // namespace Engine
