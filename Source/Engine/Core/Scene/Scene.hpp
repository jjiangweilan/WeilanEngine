#pragma once

#include "Core/Asset.hpp"
#include "Core/Component/Camera.hpp"
#include "Core/Component/Light.hpp"
#include "Core/GameObject.hpp"
#include "Core/Ptr.hpp"
#include "Core/Scene/PhysicsScene.hpp"
#include "GfxDriver/ShaderResource.hpp"
#include "Rendering/RenderPipeline/RenderPipelineSetting.hpp"
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

    Gfx::ShaderResource* GetSceneShaderResource();

    RenderingScene& GetRenderingScene() { return renderingScene; }
    PhysicsScene& GetPhysicsScene() { return physicsScene; }

    std::vector<std::unique_ptr<GameObject>>& GetGameObjects() { return gameObjects; }

    void FixUndestroiedGameObjectNotInSceneTree()
    {
        auto gos = GetAllGameObjects();

        bool nextErase = true;
        while (nextErase)
        {
            auto& gs = GetGameObjects();
            for (int i = 0; i < gs.size(); ++i)
            {
                auto& g = gs[i];
                auto iter = std::find(gos.begin(), gos.end(), g.get());
                if (iter == gos.end())
                {
                    nextErase = true;
                    gs.erase(gs.begin() + i);
                    break;
                }
                else
                {
                    nextErase = false;
                }
            }
        }
    }

    ObjPtr<Rendering::RenderPipelineSetting> GetRenderPipelineSetting() const { return renderPipelineSetting; }
    void SetRenderPipelineSetting(ObjPtr<Rendering::RenderPipelineSetting> setting)
    {
        this->renderPipelineSetting = setting;
    }

    const Rendering::DrawList& GetDrawList();

protected:
    // this should be deleted after gameObjects
    RenderingScene renderingScene;
    PhysicsScene physicsScene;

    Rendering::DrawList sceneDrawList;

    ObjPtr<Rendering::RenderPipelineSetting> renderPipelineSetting;

    std::vector<std::unique_ptr<GameObject>> gameObjects;
    std::vector<ObjPtr<GameObject>> roots;

    Camera* camera = nullptr;

    void TickGameObject(GameObject* obj);
    void PrePhysicsTickGameObject(GameObject* obj);
    void DestroyGameObjectNestedCall(GameObject* obj);

};
