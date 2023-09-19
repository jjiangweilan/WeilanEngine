#pragma once

#include "Core/Asset.hpp"
#include "Core/Component/Camera.hpp"
#include "Core/Component/Light.hpp"
#include "Core/GameObject.hpp"
#include "GfxDriver/CommandBuffer.hpp"
#include "GfxDriver/ShaderResource.hpp"
#include "Rendering/SceneRenderer.hpp"
#include <SDL2/SDL.h>
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

protected:
    std::vector<std::unique_ptr<GameObject>> gameObjects;
    std::vector<RefPtr<GameObject>> externalGameObjects;
    std::vector<RefPtr<GameObject>> roots;
    std::vector<std::function<void(SDL_Event& event)>> systemEventCallbacks;

    Camera* camera = nullptr;

    void TickGameObject(RefPtr<GameObject> obj);
};
} // namespace Engine
