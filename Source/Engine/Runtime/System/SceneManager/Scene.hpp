#pragma once

#include "Core/Asset.hpp"
#include "Runtime/Object/Component/Camera.hpp"
#include "Runtime/Object/Component/Light.hpp"
#include "Runtime/Object/GameObject/GameObject.hpp"
#include "Core/Ptr.hpp"
#include "Runtime/System/SceneManager/PhysicsScene.hpp"
#include "Driver/GfxDriver/ShaderResource.hpp"
#include "Runtime/System/Rendering/RenderPipeline/RenderPipelineSetting.hpp"
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

    const std::vector<ObjPtr<GameObject>>& GetRootObjects();

    void Tick();
    void PrePhysicsTick();
    void OnLoaded() override;

    void MoveGameObjectToRoot(GameObject* obj);
    void RemoveGameObjectFromRoot(GameObject* obj);
    void DestroyGameObject(GameObject* obj);
    std::unique_ptr<GameObject> RetrieveGameObject(GameObject* obj);

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
    void SetMainCamera(Camera* camera) { this->camera = camera; }

    RenderingScene& GetRenderingScene() { return renderingScene; }
    PhysicsScene& GetPhysicsScene() { return physicsScene; }

    auto& GetGameObjects() { return gameObjects; }

    void FixUndestroiedGameObjectNotInSceneTree();

    auto GetRenderPipelineSetting() const { return renderPipelineSetting; }
    void SetRenderPipelineSetting(const auto& val) { renderPipelineSetting = val; }

protected:
    // this should be deleted after gameObjects
    RenderingScene renderingScene;
    PhysicsScene physicsScene;

    ObjPtr<Rendering::RenderPipelineSetting> renderPipelineSetting;

    std::vector<std::unique_ptr<GameObject>> gameObjects;
    std::vector<ObjPtr<GameObject>> roots;

    Camera* camera = nullptr;

    void TickGameObject(GameObject* obj);
    void PrePhysicsTickGameObject(GameObject* obj);
    void DestroyGameObjectNestedCall(GameObject* obj);
};
