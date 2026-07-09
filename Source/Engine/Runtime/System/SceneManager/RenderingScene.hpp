#pragma once
#include "Engine/Core/Ptr.hpp"
#include "Engine/Driver/GfxDriver/CommandBuffer.hpp"
#include "Engine/Driver/GfxDriver/RayTracingContext.hpp"
#include "Engine/Runtime/System/Rendering/RenderingData.hpp"
#include "Engine/Runtime/System/Rendering/SceneEnvironmentData.hpp"
#include "Engine/Runtime/System/Rendering/Structs.hpp"
#include "Engine/Runtime/System/SceneManager/RenderingObject.hpp"
#include "RenderingObjectList.hpp"

#include "Engine/Library/DynamicArray.hpp"
#include <algorithm>
#include <glm/glm.hpp>
#include <span>

class MeshRenderer;
class SceneEnvironment;
class Terrain;
class GrassSurface;
class Cloud;
class ParticleSystem;

#define RENDERING_SCENE_OBJECT_API(Type, name, container) \
    void AddRenderObject(Type& name)                      \
    {                                                     \
        AddSpecialObject(name, container);                \
    }                                                     \
    void RemoveRenderObject(Type& name)                   \
    {                                                     \
        RemoveSpecialObject(name, container);             \
    }                                                     \
    std::span<Type*> Get##Type##s()                       \
    {                                                     \
        return container;                                 \
    }

class RenderingScene
{
public:
    RenderingScene() : rayTracingContext(nullptr)
    {
        rayTracingContext = GetGfxDriver()->CreateRayTracingContext();
    };

    RenderingScene(const RenderingScene& other) = delete;
    RenderingScene(RenderingScene&& other) = delete;

    template <class T>
        requires std::derived_from<T, RenderingObject<T>>
    RenderingObjectList::ObjectList GetRenderingObjects()
    {
        return renderingObjects.GetRenderingObjects(RenderingObject<T>::renderObjectTypeID);
    }

    RenderingObjectList::ObjectList GetRenderingObjectsByEvent(Rendering::RenderEvents event)
    {
        return renderingObjects.GetRenderingObjectsByEvent(event);
    }

    void AddRenderingObject(uint32_t objectTypeID, RenderingObjectBase* object)
    {
        renderingObjects.AddToList(objectTypeID, object);
    }

    void RemoveRenderingObject(uint32_t objectTypeID, RenderingObjectBase* object)
    {
        renderingObjects.RemoveFromList(objectTypeID, object);
    }

    void SetSceneEnvironment(SceneEnvironment& sceneEnvironment)
    {
        if (this->sceneEnvironment == nullptr)
        {
            this->sceneEnvironment = &sceneEnvironment;
        }
    }

    void RemoveSceneEnvironment(SceneEnvironment& sceneEnvironment)
    {
        if (this->sceneEnvironment == &sceneEnvironment)
        {
            this->sceneEnvironment = nullptr;
        }
    }

    void SetTerrain(Terrain& terrain) { this->terrain = &terrain; }

    void RemoveTerrain(Terrain& terrain)
    {
        if (this->terrain == &terrain)
        {
            this->terrain = nullptr;
        }
    }

    Terrain* GetTerrain() { return terrain; }

    RENDERING_SCENE_OBJECT_API(Cloud, cloud, clouds);
    RENDERING_SCENE_OBJECT_API(GrassSurface, grassSurface, grassSurfaces);
    RENDERING_SCENE_OBJECT_API(ParticleSystem, particleSystem, particleSystems);

    void AddRenderer(MeshRenderer& renderingObject)
    {
        meshRenderers.push_back(&renderingObject);
    }

    void RemoveRenderer(MeshRenderer& renderingObject)
    {
        auto iter = std::find(meshRenderers.begin(), meshRenderers.end(), &renderingObject);
        if (iter != meshRenderers.end())
        {
            std::swap(*iter, meshRenderers.back());
            meshRenderers.pop_back();
        }
    }

    std::span<MeshRenderer*> GetMeshRenderers() { return meshRenderers; }

    void AddForwardRenderer(MeshRenderer& renderingObject)
    {
        forwardMeshRenderers.push_back(&renderingObject);
    }

    void RemoveForwardRenderer(MeshRenderer& renderingObject)
    {
        auto iter = std::find(forwardMeshRenderers.begin(), forwardMeshRenderers.end(), &renderingObject);
        if (iter != forwardMeshRenderers.end())
        {
            std::swap(*iter, forwardMeshRenderers.back());
            forwardMeshRenderers.pop_back();
        }
    }

    std::span<MeshRenderer*> GetForwardMeshRenderers() { return forwardMeshRenderers; }

    // GPU-Driven object list
    void AddGPUObjectRenderer(MeshRenderer& renderer)
    {
        gpuObjectRenderers.push_back(&renderer);
    }

    void RemoveGPUObjectRenderer(MeshRenderer& renderer)
    {
        auto iter = std::find(gpuObjectRenderers.begin(), gpuObjectRenderers.end(), &renderer);
        if (iter != gpuObjectRenderers.end())
        {
            std::swap(*iter, gpuObjectRenderers.back());
            gpuObjectRenderers.pop_back();
        }
    }

    std::span<MeshRenderer*> GetGPUObjectRenderers() { return gpuObjectRenderers; }

    SceneEnvironmentData& GetSceneEnvironmentData();

    Gfx::RayTracingContext* GetRayTracingContext() { return rayTracingContext.get(); }
    Gfx::RayTracingSceneHandle GetRayTracingSceneHandle() { return rayTracingScene; }

    Gfx::RayTracingMeshHandle CreateBLAS(std::span<Gfx::BlasGeometry> geometries);
    Gfx::RayTracingInstanceHandle CreateInstance(MeshRenderer* renderer, Gfx::RayTracingMeshHandle mesh, glm::float4x3 transform);
    void UpdateRayTracingInstance(Gfx::RayTracingInstanceHandle instance, glm::float4x3 transform);

    void Tick();
    void ResetRuntimeState();

private:
    struct RTInstance
    {
        Gfx::RayTracingInstanceHandle handle;
        MeshRenderer* renderer;
    };

    Scene* scene;
    RenderingObjectList renderingObjects;
    std::unique_ptr<Gfx::RayTracingContext> rayTracingContext;
    Gfx::RayTracingSceneHandle rayTracingScene = 0;
    std::vector<RTInstance> rayTracingInstances;
    std::unique_ptr<Gfx::Buffer> rtObjectOffsetsBuffer;
    bool needsTLASRebuild = false;

    template <class T>
    void AddSpecialObject(T& obj, std::vector<T*>& addTo)
    {
        addTo.push_back(&obj);
    }

    template <class T>
    void RemoveSpecialObject(T& obj, std::vector<T*>& removeFrom)
    {
        auto iter = std::find(removeFrom.begin(), removeFrom.end(), &obj);
        if (iter != removeFrom.end())
        {
            std::swap(*iter, removeFrom.back());
            removeFrom.pop_back();
        }
    }

    std::vector<ParticleSystem*> particleSystems;
    std::vector<MeshRenderer*> meshRenderers;
    std::vector<MeshRenderer*> forwardMeshRenderers;
    std::vector<MeshRenderer*> gpuObjectRenderers;
    std::vector<GrassSurface*> grassSurfaces;
    std::vector<Cloud*> clouds;

    SceneEnvironment* sceneEnvironment = nullptr;
    Terrain* terrain = nullptr;

    friend class Scene;
};
