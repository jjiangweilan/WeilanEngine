#include "RenderSceneBVH.hpp"

#include "Engine/Library/Math.hpp"
#include "Engine/MiddleLayer/EngineDebug.hpp"
#include "Engine/MiddleLayer/EngineInternalResources.hpp"

#include "Engine/ThirdParty/imgui/imgui.h"

void RenderSceneBVH::Build(RenderInstance** bvhObjects, int objectsCount, int maxNodeLevel)
{
    nodes.clear();
    objects.clear();
    objectMap.clear();
    objectToLeafIndex.clear();
    objectToLeafIndex.resize(objectsCount, -1);

    int totalNodes = (glm::pow(2, maxNodeLevel) - 1);
    this->nodes.resize(totalNodes);
    maxNonLeafNodeIndex = glm::pow(2, maxNodeLevel - 1) - 2;

    std::vector<glm::float3> objectCenters;
    objectCenters.reserve(objectsCount);

    int rootNodeIndex = 0;
    for (int objectIndex = 0; objectIndex < objectsCount; ++objectIndex)
    {
        nodes[rootNodeIndex].objectIndices.push_back(objectIndex);
        objects.push_back(bvhObjects[objectIndex]);
        objectCenters.push_back(bvhObjects[objectIndex]->GetAABB().GetCenter());
        objectMap[bvhObjects[objectIndex]] = objectIndex;
    }

    for (int i = 0; i < nodes.size(); i++)
    {
        UpdateNode(i, objectCenters);
    }

    for (int i = 0; i < nodes.size(); ++i)
    {
        if (nodes[i].IsLeaf() && !nodes[i].IsEmpty())
        {
            for (int objIdx : nodes[i].objectIndices)
            {
                objectToLeafIndex[objIdx] = i;
            }
        }
    }
}

void RenderSceneBVH::AppendRefitObject(RenderInstance* object)
{
    auto it = objectMap.find(object);
    if (it != objectMap.end())
    {
        int objIdx = it->second;
        if (objIdx < objectToLeafIndex.size())
        {
            int leafIdx = objectToLeafIndex[objIdx];
            if (leafIdx != -1)
            {
                pendingRefit.insert(leafIdx);
            }
        }
    }
}

void RenderSceneBVH::Refit(int nodeIndex)
{
    int curr = nodeIndex;
    while (curr != -1)
    {
        Node& node = nodes[curr];
        if (node.IsLeaf())
        {
            UpdateNodeBounds(curr);
        }
        else
        {
            node.aabb.min = glm::float3(std::numeric_limits<float>::max());
            node.aabb.max = glm::float3(std::numeric_limits<float>::lowest());

            if (node.HasLeftChild())
            {
                const auto& childAABB = nodes[node.childNodeLeft].aabb;
                node.aabb.min = glm::min(node.aabb.min, childAABB.min);
                node.aabb.max = glm::max(node.aabb.max, childAABB.max);
            }
            if (node.HasRightChild())
            {
                const auto& childAABB = nodes[node.childNodeRight].aabb;
                node.aabb.min = glm::min(node.aabb.min, childAABB.min);
                node.aabb.max = glm::max(node.aabb.max, childAABB.max);
            }
        }
        curr = node.parentIndex;
    }
}

void RenderSceneBVH::UpdateNodeBounds(int nodeIndex)
{
    Node& node = nodes[nodeIndex];

    if (!node.objectIndices.empty())
    {
        node.aabb.min = glm::float3(std::numeric_limits<float>::max());
        node.aabb.max = glm::float3(std::numeric_limits<float>::lowest());
        for (auto idx : node.objectIndices)
        {
            node.aabb.min = glm::min(node.aabb.min, objects[idx]->GetAABB().min);
            node.aabb.max = glm::max(node.aabb.max, objects[idx]->GetAABB().max);
        }
    }
}

std::vector<RenderSceneBVH::Node*> RenderSceneBVH::QueryNodesInFrustum(const Frustum& Frustum)
{
    std::vector<RenderSceneBVH::Node*> result{};

    if (!nodes.empty())
    {
        auto& root = GetRoot();
        QueryNodesInFrustum(Frustum, root, result);
    }

    return result;
}

void RenderSceneBVH::QueryNodesInFrustum(
    const Frustum& Frustum, Node& node, std::vector<RenderSceneBVH::Node*>& inFrustum
)
{
    // TODO(perf): we should be able to reuse the dot calculation in these two test functions

    if (node.IsFullyVisibleInFrustum(Frustum))
    {
        inFrustum.push_back(&node);
    }
    else if (node.IsVisibleInFrustum(node.aabb, Frustum))
    {
        if (node.HasLeftChild())
            QueryNodesInFrustum(Frustum, nodes[node.childNodeLeft], inFrustum);

        if (node.HasRightChild())
            QueryNodesInFrustum(Frustum, nodes[node.childNodeRight], inFrustum);

        if (node.IsLeaf())
        {
            inFrustum.push_back(&node);
        }
    }
}

void RenderSceneBVH::UpdateNode(int nodeIndex, const std::vector<glm::float3>& objectCenters)
{
    Node& node = nodes[nodeIndex];

    UpdateNodeBounds(nodeIndex);

    if (nodeIndex > maxNonLeafNodeIndex || node.objectIndices.size() <= 1)
    {
        return;
    }

    node.childNodeLeft = nodeIndex * 2 + 1;
    node.childNodeRight = nodeIndex * 2 + 2;

    glm::float3 extent = node.aabb.max - node.aabb.min;
    float longestAxisLength = extent.x;
    int longestAxis = 0;
    if (extent.y > longestAxisLength)
    {
        longestAxis = 1;
        longestAxisLength = extent.y;
    }
    if (extent.z > longestAxisLength)
    {
        longestAxis = 2;
    }

    float separationPlane = 0;
    for (auto objIdx : node.objectIndices)
    {
        separationPlane += objectCenters[objIdx][longestAxis];
    }
    separationPlane /= node.objectIndices.size();

    for (auto objIdx : node.objectIndices)
    {
        int childIndex =
            objectCenters[objIdx][longestAxis] < separationPlane ? node.childNodeLeft : node.childNodeRight;
        ASSERT(childIndex < nodes.size());
        nodes[childIndex].objectIndices.push_back(objIdx);
        nodes[childIndex].parentIndex = nodeIndex;
    }
}

void RenderSceneBVH::Refit()
{
    for (auto nodeIdx : pendingRefit)
    {
        Refit(nodeIdx);
    }
    pendingRefit.clear();
}

bool RenderSceneBVH::Node::IsFullyVisibleInFrustum(const Frustum& Frustum)
{
    const glm::vec3& vmin = aabb.min;
    const glm::vec3& vmax = aabb.max;

    for (size_t i = 0; i < 6; ++i)
    {
        const glm::vec4& g = Frustum.planes[i];
        if ((glm::dot(g, glm::vec4(vmin.x, vmin.y, vmin.z, 1.0f)) < 0.0) ||
            (glm::dot(g, glm::vec4(vmax.x, vmin.y, vmin.z, 1.0f)) < 0.0) ||
            (glm::dot(g, glm::vec4(vmin.x, vmax.y, vmin.z, 1.0f)) < 0.0) ||
            (glm::dot(g, glm::vec4(vmax.x, vmax.y, vmin.z, 1.0f)) < 0.0) ||
            (glm::dot(g, glm::vec4(vmin.x, vmin.y, vmax.z, 1.0f)) < 0.0) ||
            (glm::dot(g, glm::vec4(vmax.x, vmin.y, vmax.z, 1.0f)) < 0.0) ||
            (glm::dot(g, glm::vec4(vmin.x, vmax.y, vmax.z, 1.0f)) < 0.0) ||
            (glm::dot(g, glm::vec4(vmax.x, vmax.y, vmax.z, 1.0f)) < 0.0))
        {
            // One of the vertices is outside
            return false;
        }
    }

    return true;
}

bool RenderSceneBVH::Node::IsVisibleInFrustum(const AABB& aabb, const Frustum& Frustum)
{
    const glm::vec3& vmin = aabb.min;
    const glm::vec3& vmax = aabb.max;

    for (size_t i = 0; i < 6; ++i)
    {
        const glm::vec4& g = Frustum.planes[i];
        if ((glm::dot(g, glm::vec4(vmin.x, vmin.y, vmin.z, 1.0f)) < 0.0) &&
            (glm::dot(g, glm::vec4(vmax.x, vmin.y, vmin.z, 1.0f)) < 0.0) &&
            (glm::dot(g, glm::vec4(vmin.x, vmax.y, vmin.z, 1.0f)) < 0.0) &&
            (glm::dot(g, glm::vec4(vmax.x, vmax.y, vmin.z, 1.0f)) < 0.0) &&
            (glm::dot(g, glm::vec4(vmin.x, vmin.y, vmax.z, 1.0f)) < 0.0) &&
            (glm::dot(g, glm::vec4(vmax.x, vmin.y, vmax.z, 1.0f)) < 0.0) &&
            (glm::dot(g, glm::vec4(vmin.x, vmax.y, vmax.z, 1.0f)) < 0.0) &&
            (glm::dot(g, glm::vec4(vmax.x, vmax.y, vmax.z, 1.0f)) < 0.0))
        {
            // Completely outside the frustum
            return false;
        }
    }

    return true;
}

std::vector<RenderInstance*> RenderSceneBVH::QueryRendererInFrustum(const Frustum& frustum)
{
    std::vector<RenderInstance*> objs{};
    auto nodes = QueryNodesInFrustum(frustum);
    for (auto n : nodes)
    {
        for (auto objIdx : n->objectIndices)
        {
            auto obj = objects[objIdx];
            if (obj && Node::IsVisibleInFrustum(obj->GetAABB(), frustum))
                objs.push_back(obj);
        }
    }

    return objs;
}
