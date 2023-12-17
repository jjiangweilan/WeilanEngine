#pragma once

#include "Core/Asset.hpp"
#include "Core/Component/Camera.hpp"
#include "Core/Component/Light.hpp"
#include "Core/GameObject.hpp"
#include "GfxDriver/CommandBuffer.hpp"
#include "GfxDriver/ShaderResource.hpp"
#include "RenderingScene.hpp"
#include <SDL2/SDL.h>
#include <iterator>
namespace Engine
{
class Scene : public Asset
{
    DECLARE_ASSET();

public:
    Scene();
    ~Scene() {}
    GameObject* CreateGameObject();
    void AddGameObject(GameObject* newGameObject);
    GameObject* AddGameObject(std::unique_ptr<GameObject>&& newGameObject);
    void AddGameObjects(std::vector<std::unique_ptr<GameObject>>&& gameObjects);
    GameObject* CopyGameObject(GameObject& gameObject);

    const std::vector<GameObject*>& GetRootObjects();

    void Tick();

    void MoveGameObjectToRoot(GameObject* obj);
    void RemoveGameObjectFromRoot(GameObject* obj);
    void RemoveGameObject(GameObject* obj);
    void DestroyGameObject(GameObject* obj);

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

    void InvokeSystemEventCallbacks(SDL_Event& event)
    {
        for (auto& cb : systemEventCallbacks)
        {
            cb(event);
        }
    }

    void RegisterSystemEventCallback(const std::function<void(SDL_Event& event)>& cb)
    {
        systemEventCallbacks.push_back(cb);
    }

    RenderingScene& GetRenderingScene()
    {
        return renderingScene;
    }

protected:
    // this should be deleted after gameObjects
    RenderingScene renderingScene;

    std::vector<std::unique_ptr<GameObject>> gameObjects;
    std::vector<GameObject*> externalGameObjects;
    std::vector<GameObject*> roots;
    std::vector<std::function<void(SDL_Event& event)>> systemEventCallbacks;

    Camera* camera = nullptr;

    void TickGameObject(GameObject* obj);
};
} // namespace Engine
