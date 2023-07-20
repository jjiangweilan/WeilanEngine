#pragma once

#include "Core/Component/Camera.hpp"
#include "Core/Component/Light.hpp"
#include "Core/GameObject.hpp"
#include "Core/Resource.hpp"
namespace Engine
{
class Scene : public Resource
{
public:
    Scene();
    ~Scene() {}
    GameObject* CreateGameObject();
    void AddGameObject(GameObject* newGameObject);

    const std::vector<RefPtr<GameObject>>& GetRootObjects();

    void Tick();

    void MoveGameObjectToRoot(RefPtr<GameObject> obj);
    void RemoveGameObjectFromRoot(RefPtr<GameObject> obj);
    void RemoveGameObject(GameObject* obj);

    std::vector<RefPtr<Light>> GetActiveLights();

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    Camera* GetMainCamera()
    {
        return camera;
    }
    void SetMainCamera(Camera* camera)
    {
        this->camera = camera;
    }

protected:
    std::vector<UniPtr<GameObject>> gameObjects;
    std::vector<RefPtr<GameObject>> externalGameObjects;
    std::vector<RefPtr<GameObject>> roots;

    Camera* camera;

    void TickGameObject(RefPtr<GameObject> obj);
};
} // namespace Engine
