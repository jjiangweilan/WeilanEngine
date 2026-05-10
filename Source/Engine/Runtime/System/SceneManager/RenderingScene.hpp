#pragma once
#include "Engine/Core/Ptr.hpp"
#include "Engine/Driver/GfxDriver/CommandBuffer.hpp"
#include "Engine/Driver/GfxDriver/RayTracingContext.hpp"
#include "Engine/Library/Math.hpp"
#include "Engine/Runtime/System/Rendering/RenderingData.hpp"
#include "Engine/Runtime/System/Rendering/SceneEnvironmentData.hpp"
#include "Engine/Runtime/System/Rendering/Structs.hpp"
#include "Engine/Runtime/System/SceneManager/RenderingObject.hpp"
#include "RenderingObjectList.hpp"

#include "Engine/Library/DynamicArray.hpp"
#include <algorithm>
#include <glm/glm.hpp>
#include <set>
#include <span>
#include <unordered_map>

class MeshRenderer;
class Material;
class Mesh;
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

struct BoundingVolumeHierarchy
{
public:
    struct Node
    {
        AABB aabb{};

        int parentIndex = -1;
        int childNodeLeft = -1;
        int childNodeRight = -1;
        bool IsLeaf() const { return childNodeLeft == -1 && childNodeRight == -1; }
        bool IsEmpty() const { return objectIndices.empty(); }
        bool HasLeftChild() const { return childNodeLeft != -1; }
        bool HasRightChild() const { return childNodeRight != -1; }

        std::vector<int> objectIndices{};

        static bool IsVisibleInFrustum(const AABB& aabb, const Frustum& frustum);
        bool IsFullyVisibleInFrustum(const Frustum& frustum);
    };

    std::vector<MeshRenderer*> QueryRendererInFrustum(const Frustum& frustum);
    std::vector<Node*> QueryNodesInFrustum(const Frustum& frustum);
    void Build(MeshRenderer** bvhObjects, int objectsCount, int maxNodeLevel);

    Node& GetRoot() { return nodes[0]; }

    std::vector<Node> nodes{};
    std::vector<ObjPtr<MeshRenderer>> objects{};
    std::vector<glm::float3> objectCenters{};
    int maxNonLeafNodeIndex = 0;

    void UpdateNodeBounds(int nodeIndex);
    void UpdateNode(int nodeIndex);

    void AppendRefitObject(MeshRenderer* object);
    void Refit();

private:
    void QueryNodesInFrustum(
        const Frustum& Frustum, Node& node, std::vector<BoundingVolumeHierarchy::Node*>& inFrustum
    );

    void Refit(int nodeIndex);
    std::unordered_map<MeshRenderer*, int> objectMap;
    std::vector<int> objectToLeafIndex;
    std::set<int> pendingRefit;
};

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

    RenderingObjectList::ObjectIndex AddRenderingObject(uint32_t objectTypeID, RenderingObjectBase* object)
    {
        return renderingObjects.AddToList(objectTypeID, object);
    }

    void RemoveRenderingObject(uint32_t objectTypeID, RenderingObjectList::ObjectIndex index)
    {
        renderingObjects.RemoveFromList(objectTypeID, index);
    }

    std::vector<MeshRenderer*> QueryRendererInFrustum(const Frustum& frustum)
    {
        return rendererNodeHierarchy.QueryRendererInFrustum(frustum);
    }

    std::vector<BoundingVolumeHierarchy::Node*> QueryNodesInFrustum(const Frustum& frustum)
    {
        return rendererNodeHierarchy.QueryNodesInFrustum(frustum);
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

    void RebuildBVH() { updateRendererNodeHierarchy = true; }
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
        updateRendererNodeHierarchy = true;
    }

    void RemoveRenderer(MeshRenderer& renderingObject)
    {
        auto iter = std::find(meshRenderers.begin(), meshRenderers.end(), &renderingObject);
        if (iter != meshRenderers.end())
        {
            std::swap(*iter, meshRenderers.back());
            meshRenderers.pop_back();
        }
        updateRendererNodeHierarchy = true;
    }

    void UpdateRenderer(MeshRenderer& renderingObject)
    {
        rendererNodeHierarchy.AppendRefitObject(&renderingObject);
    }

    std::span<MeshRenderer*> GetMeshRenderers() { return meshRenderers; }

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
    std::vector<MeshRenderer*> gpuObjectRenderers;
    std::vector<GrassSurface*> grassSurfaces;
    std::vector<Cloud*> clouds;

    SceneEnvironment* sceneEnvironment = nullptr;
    Terrain* terrain = nullptr;

    BoundingVolumeHierarchy rendererNodeHierarchy;
    bool updateRendererNodeHierarchy = false;

    void BVHDebug();
    Mesh* GetBVHDebugMesh();
    Material& GetBVHDebugMaterial();

    Mesh* bvhDebugMesh = nullptr;
    std::unique_ptr<Material> bvhDebugMaterial;

    friend class Scene;
};
