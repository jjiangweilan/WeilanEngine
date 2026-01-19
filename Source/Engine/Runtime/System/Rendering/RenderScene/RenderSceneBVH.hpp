#pragma once

#include "Engine/Library/Math.hpp"
#include "Engine/Library/Math/Geometry/Geometry.hpp"
#include "RenderInstance.hpp"
#include <set>

struct RenderSceneBVH
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

    std::vector<RenderInstance*> QueryRendererInFrustum(const Frustum& frustum);
    std::vector<Node*> QueryNodesInFrustum(const Frustum& frustum);
    void Build(RenderInstance** bvhObjects, int objectsCount, int maxNodeLevel);

    Node& GetRoot() { return nodes[0]; }

    std::vector<Node> nodes{};
    std::vector<RenderInstance*> objects{};
    int maxNonLeafNodeIndex = 0;

    void AppendRefitObject(RenderInstance* object);
    void Refit();

private:
    void UpdateNodeBounds(int nodeIndex);
    void UpdateNode(int nodeIndex, const std::vector<glm::float3>& objectCenters);

    void QueryNodesInFrustum(
        const Frustum& Frustum, Node& node, std::vector<Node*>& inFrustum
    );

    void Refit(int nodeIndex);
    std::unordered_map<RenderInstance*, int> objectMap;
    std::vector<int> objectToLeafIndex;
    std::set<int> pendingRefit;
};
