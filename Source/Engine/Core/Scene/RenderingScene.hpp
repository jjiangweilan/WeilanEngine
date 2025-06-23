#pragma once
#include "Core/Ptr.hpp"
#include "GfxDriver/CommandBuffer.hpp"
#include "Libs/Math.hpp"
#include "Rendering/RenderingData.hpp"
#include "Rendering/Structs.hpp"

#include "Libs/DynamicArray.hpp"
#include <algorithm>
#include <glm/glm.hpp>
#include <span>

class MeshRenderer;
class SceneEnvironment;
class Terrain;
class GrassSurface;
class Cloud;
class ParticleSystem;
class ReflectionProbe;

#define RENDERING_SCENE_OBJECT_API(Type, name, container)                                                              \
    void AddRenderObject(Type& name)                                                                                   \
    {                                                                                                                  \
        AddSpecialObject(name, container);                                                                             \
    }                                                                                                                  \
    void RemoveRenderObject(Type& name)                                                                                \
    {                                                                                                                  \
        RemoveSpecialObject(name, container);                                                                          \
    }                                                                                                                  \
    std::span<Type*> Get##Type##s()                                                                                    \
    {                                                                                                                  \
        return container;                                                                                              \
    }

struct BoundingVolumeHierarchy
{
public:
    struct Node
    {
        AABB aabb{};

        int childNodeLeft = -1;
        int childNodeRight = -1;
        bool IsLeaf() const { return childNodeLeft == -1 && childNodeRight == -1; }
        bool IsEmpty() const { return objectIndices.empty(); }
        bool HasLeftChild() const { return childNodeLeft != -1; }
        bool HasRightChild() const { return childNodeRight != -1; }

        DynamicArray<int> objectIndices{};

        static bool IsVisibleInFrustum(const AABB& aabb, const Frustum& frustum);
        bool IsFullyVisibleInFrustum(const Frustum& frustum);
    };

    DynamicArray<MeshRenderer*> QueryRendererInFrustum(const Frustum& frustum);
    DynamicArray<Node*> QueryNodesInFrustum(const Frustum& frustum);
    void Build(MeshRenderer** bvhObjects, int objectsCount, int maxNodeLevel);

    Node& GetRoot() { return nodes[0]; }

    DynamicArray<Node> nodes{};
    DynamicArray<ObjPtr<MeshRenderer>> objects{};
    DynamicArray<glm::float3> objectCenters{};
    int maxNonLeafNodeIndex = 0;

    void UpdateNodeBounds(int nodeIndex);
    void UpdateNode(int nodeIndex);

private:
    void QueryNodesInFrustum(
        const Frustum& Frustum, Node& node, DynamicArray<BoundingVolumeHierarchy::Node*>& inFrustum
    );
};

class RenderingScene
{
public:
    RenderingScene() {};
    RenderingScene(const RenderingScene& other) = delete;
    RenderingScene(RenderingScene&& other) = delete;

    DynamicArray<MeshRenderer*> QueryRendererInFrustum(const Frustum& frustum)
    {
        return rendererNodeHierarchy.QueryRendererInFrustum(frustum);
    }

    DynamicArray<BoundingVolumeHierarchy::Node*> QueryNodesInFrustum(const Frustum& frustum)
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
    RENDERING_SCENE_OBJECT_API(ReflectionProbe, reflectionProbe, reflectionProbes);

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

    std::span<MeshRenderer*> GetMeshRenderers() { return meshRenderers; }

    SceneEnvironment* GetSceneEnvironment() { return sceneEnvironment; }

    void Tick();

private:
    Scene* scene;

    template <class T>
    void AddSpecialObject(T& obj, DynamicArray<T*>& addTo)
    {
        addTo.push_back(&obj);
    }

    template <class T>
    void RemoveSpecialObject(T& obj, DynamicArray<T*>& removeFrom)
    {
        auto iter = std::find(removeFrom.begin(), removeFrom.end(), &obj);
        if (iter != removeFrom.end())
        {
            std::swap(*iter, removeFrom.back());
            removeFrom.pop_back();
        }
    }

    DynamicArray<ReflectionProbe*> reflectionProbes;
    DynamicArray<ParticleSystem*> particleSystems;
    DynamicArray<MeshRenderer*> meshRenderers;
    DynamicArray<GrassSurface*> grassSurfaces;
    DynamicArray<Cloud*> clouds;

    SceneEnvironment* sceneEnvironment = nullptr;
    Terrain* terrain = nullptr;

    BoundingVolumeHierarchy rendererNodeHierarchy;
    bool updateRendererNodeHierarchy = false;

    void BVHDebug();

    friend class Scene;
};
