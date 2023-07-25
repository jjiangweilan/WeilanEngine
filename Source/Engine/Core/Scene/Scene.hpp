#pragma once

#include "Core/Component/Camera.hpp"
#include "Core/Component/Light.hpp"
#include "Core/GameObject.hpp"
#include "Core/Resource.hpp"
#include "GfxDriver/CommandBuffer.hpp"
#include "GfxDriver/ShaderResource.hpp"
#include "Rendering/DualMoonRenderer.hpp"
#include <SDL2/SDL.h>
namespace Engine
{
class Scene : public Resource
{
public:
    struct CreateInfo
    {
        std::function<void(Gfx::CommandBuffer& cmd)> render;
    };
    Scene(const CreateInfo& createInfo);
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

    void Render(Gfx::CommandBuffer& cmd)
    {
        render(cmd);
    }

protected:
    std::unique_ptr<DualMoonRenderer> sceneRenderer;
    std::vector<UniPtr<GameObject>> gameObjects;
    std::vector<RefPtr<GameObject>> externalGameObjects;
    std::vector<RefPtr<GameObject>> roots;
    std::vector<std::function<void(SDL_Event& event)>> systemEventCallbacks;
    std::function<void(Gfx::CommandBuffer&)> render;

    Camera* camera;

    void TickGameObject(RefPtr<GameObject> obj);
};
} // namespace Engine
