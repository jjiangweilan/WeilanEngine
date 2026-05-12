#pragma once

#include "Engine/Core/Asset.hpp"
#include "Engine/Core/Ptr.hpp"
#include "Engine/Driver/GfxDriver/ShaderResource.hpp"
#include "Engine/Runtime/Object/Component/Camera.hpp"
#include "Engine/Runtime/Object/Component/Light.hpp"
#include "Engine/Runtime/Object/GameObject/GameObject.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipelineSetting.hpp"
#include "Engine/Runtime/System/SceneManager/BVHScene.hpp"
#include "Engine/Runtime/System/SceneManager/PhysicsScene.hpp"
#include "RenderingScene.hpp"
#include <SDL.h>
#include <type_traits>

class [[LuaClass]] Scene : public Asset
{
    DECLARE_ASSET();

public:
    Scene();
    ~Scene();
    GameObject* CreateGameObject();
    GameObject* AddGameObject(std::unique_ptr<GameObject>&& newGameObject);
    void AddGameObjects(std::vector<std::unique_ptr<GameObject>>&& gameObjects);
    GameObject* CopyGameObject(GameObject& gameObject);

    [[LuaNamedFn("CreateGameObject")]] ObjPtr<GameObject> Lua_CreateGameObject();

    const std::vector<ObjPtr<GameObject>>& GetRootObjects();

    void Tick();
    void PrePhysicsTick();
    void OnLoaded() override;

    void MoveGameObjectToRoot(GameObject* obj);
    void MoveRootGameObjectToIndex(GameObject* obj, int index);
    void RemoveGameObjectFromRoot(GameObject* obj);
    [[LuaFn]] ObjPtr<GameObject> SpawnPrefab(const ObjPtr<Prefab>& prefab);
    [[LuaFn]] void DestroyGameObject(GameObject* obj);
    std::unique_ptr<GameObject> RetrieveGameObject(GameObject* obj);

    [[deprecated("we can't actually clone a scene, the internal reference is hard to resolve. Use "
                 "AssetDatabse::CopyThroughSerialization instead")]]
    std::unique_ptr<Asset> Clone() override;

    std::vector<GameObject*> GetAllGameObjects();
    template <class Func>
    bool ForEachGameObject(Func&& func)
    {
        for (auto& obj : roots)
        {
            if (!ForEachGameObject(obj, func))
                return false;
        }

        return true;
    }
    std::vector<Light*> GetActiveLights();

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    void ResetRuntimeAndSerializedState();
    Camera* GetMainCamera()
    {
        if (camera == nullptr)
        {
            ForEachGameObject([this](GameObject* go)
            {
                auto cam = go->GetComponent<Camera>();
                if (cam)
                {
                    camera = cam;
                    return false;
                }
                return true;
            });
        }
        return camera;
    }
    void SetMainCamera(Camera* camera) { this->camera = camera; }

    RenderingScene& GetRenderingScene() { return renderingScene; }
    BVHScene& GetBVHScene() { return bvhScene; }
    PhysicsScene& GetPhysicsScene() { return physicsScene; }

    auto& GetGameObjects() { return gameObjects; }

    void FixUndestroiedGameObjectNotInSceneTree();

    auto GetRenderPipelineSetting() const { return renderPipelineSetting; }
    void SetRenderPipelineSetting(const auto& val) { renderPipelineSetting = val; }

protected:
    // this should be deleted after gameObjects
    RenderingScene renderingScene;
    BVHScene bvhScene;
    PhysicsScene physicsScene;

    ObjPtr<Rendering::RenderPipelineSetting> renderPipelineSetting;

    std::vector<std::unique_ptr<GameObject>> gameObjects;
    std::vector<ObjPtr<GameObject>> roots;

    Camera* camera = nullptr;

    void TickGameObject(GameObject* obj);
    void PrePhysicsTickGameObject(GameObject* obj);
    void DestroyGameObjectNestedCall(GameObject* obj);

    template <class Func>
    bool ForEachGameObject(GameObject* current, Func& func)
    {
        if (current == nullptr)
            return true;

        using Result = std::invoke_result_t<Func&, GameObject*>;
        if constexpr (std::is_same_v<Result, bool>)
        {
            if (!func(current))
                return false;
        }
        else
        {
            func(current);
        }

        for (auto& child : current->GetChildren())
        {
            if (!ForEachGameObject(child, func))
                return false;
        }

        return true;
    }
};
