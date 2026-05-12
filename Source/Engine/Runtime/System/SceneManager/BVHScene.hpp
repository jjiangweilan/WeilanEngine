#pragma once

#include "Engine/Library/Math.hpp"
#include "Engine/Library/Math/Geometry/Geometry.hpp"
#include "Engine/Runtime/System/Rendering/Structs.hpp"
#include <functional>
#include <limits>
#include <memory>
#include <set>
#include <span>
#include <vector>

class Material;
class Mesh;
class MeshRenderer;
class Scene;

enum class BVHNodeType
{
    MeshRenderer,
    Other,
};

struct BVHNode
{
    BVHNodeType type = BVHNodeType::Other;
    std::function<bool()> isValid;
    std::function<AABB()> getAABB;
    void* owner = nullptr;
};

struct BVHHandle
{
    uint32_t index = std::numeric_limits<uint32_t>::max();
    uint32_t generation = 0;

    bool IsValid() const { return index != std::numeric_limits<uint32_t>::max(); }
};

class BVHScene
{
public:
    struct HierarchyNode
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

    explicit BVHScene(Scene* scene = nullptr) : scene(scene) {}

    BVHHandle AddNode(BVHNode node);
    void RemoveNode(BVHHandle handle);
    void MarkDirty(BVHHandle handle);
    void Tick();
    void UpdateHierarchy();
    void ResetRuntimeState();

    std::vector<BVHNode*> QueryRay(
        const Ray& ray,
        BVHNodeType type,
        float maxDistance = std::numeric_limits<float>::max()
    );
    std::vector<HierarchyNode*> QueryNodesInFrustum(const Frustum& frustum);
    std::vector<BVHNode*> QueryInFrustum(const Frustum& frustum, BVHNodeType type);
    std::vector<MeshRenderer*> QueryMeshRenderersInFrustum(const Frustum& frustum);

    const std::vector<HierarchyNode>& GetNodes() const { return nodes; }

private:
    struct Entry
    {
        BVHNode node;
        uint32_t generation = 1;
        bool active = false;
        int leafIndex = -1;
    };

    Scene* scene = nullptr;
    std::vector<HierarchyNode> nodes{};
    std::vector<Entry> entries{};
    std::vector<uint32_t> freeEntryIndices{};
    std::vector<BVHHandle> objects{};
    std::vector<glm::float3> objectCenters{};
    std::set<uint32_t> pendingRefit{};
    bool needsRebuild = false;
    int maxNonLeafNodeIndex = 0;

    bool IsHandleAlive(BVHHandle handle) const;
    BVHNode* GetNode(BVHHandle handle);
    AABB GetObjectAABB(int objectIndex);
    bool IsObjectValid(int objectIndex) const;
    HierarchyNode& GetRoot() { return nodes[0]; }

    void Build(int maxNodeLevel);
    void ClearHierarchy();
    void UpdateNodeBounds(int nodeIndex);
    void UpdateNode(int nodeIndex);
    void Refit();
    void Refit(int nodeIndex);
    void QueryRay(const Ray& ray, HierarchyNode& node, BVHNodeType type, float maxDistance, std::vector<BVHNode*>& hits);
    void QueryNodesInFrustum(const Frustum& frustum, HierarchyNode& node, std::vector<HierarchyNode*>& inFrustum);

    void BVHDebug();
    Mesh* GetBVHDebugMesh();
    Material& GetBVHDebugMaterial();

    Mesh* bvhDebugMesh = nullptr;
    std::unique_ptr<Material> bvhDebugMaterial;
};
