#pragma once

#include "Core/Asset.hpp"
#include "Core/Component/Camera.hpp"
#include "Core/Component/Light.hpp"
#include "Core/GameObject.hpp"
#include "Core/Scene/PhysicsScene.hpp"
#include "GfxDriver/ShaderResource.hpp"
#include "RenderingScene.hpp"
#include <SDL.h>

class Scene : public Asset
{
    DECLARE_ASSET();

public:
    Scene();
    ~Scene();
    GameObject* CreateGameObject();
    GameObject* AddGameObject(std::unique_ptr<GameObject>&& newGameObject);
    void AddGameObjects(std::vector<std::unique_ptr<GameObject>>&& gameObjects);
    GameObject* CopyGameObject(GameObject& gameObject);

    const std::vector<GameObject*>& GetRootObjects();

    void Tick();

    void MoveGameObjectToRoot(GameObject* obj);
    void RemoveGameObjectFromRoot(GameObject* obj);
    void RemoveGameObject(GameObject* obj);
    void DestroyGameObject(GameObject* obj);

    [[deprecated("we can't actually clone a scene, the internal reference is hard to resolve. Use "
                 "AssetDatabse::CopyThroughSerialization instead")]]
    std::unique_ptr<Asset> Clone() override;

    std::vector<GameObject*> GetAllGameObjects();

    std::vector<Light*> GetActiveLights();

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    Camera* GetMainCamera()
    {
        if (camera == nullptr)
        {
            for (auto go : GetAllGameObjects())
            {
                auto cam = go->GetComponent<Camera>();
                if (cam)
                {
                    camera = cam;
                    break;
                }
            }
        }
        return camera;
    }
    void SetMainCamera(Camera* camera)
    {
        this->camera = camera;
    }

    Gfx::ShaderResource* GetSceneShaderResource();

    RenderingScene& GetRenderingScene()
    {
        return renderingScene;
    }

    PhysicsScene& GetPhysicsScene()
    {
        return physicsScene;
    }

protected:
    // this should be deleted after gameObjects
    RenderingScene renderingScene;
    PhysicsScene physicsScene;

    std::vector<std::unique_ptr<GameObject>> gameObjects;
    std::vector<GameObject*> roots;

    Camera* camera = nullptr;

    void TickGameObject(GameObject* obj);
};
