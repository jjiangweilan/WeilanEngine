#include "BVHScene.hpp"
#include "Engine/MiddleLayer/EngineDebug.hpp"
#include "Engine/MiddleLayer/EngineInternalResources.hpp"
#include "Engine/Runtime/Object/Component/Camera.hpp"
#include "Engine/Runtime/Object/Component/MeshRenderer.hpp"
#include "Engine/Runtime/System/Rendering/Graphics.hpp"
#include "Engine/Runtime/System/Rendering/Material.hpp"
#include "Engine/Runtime/System/SceneManager/Scene.hpp"
#include "Engine/ThirdParty/imgui/imgui.h"

BVHHandle BVHScene::AddNode(BVHNode node)
{
    uint32_t index;
    if (!freeEntryIndices.empty())
    {
        index = freeEntryIndices.back();
        freeEntryIndices.pop_back();
        entries[index].node = std::move(node);
        entries[index].active = true;
        entries[index].leafIndex = -1;
    }
    else
    {
        index = static_cast<uint32_t>(entries.size());
        entries.push_back({.node = std::move(node), .generation = 1, .active = true, .leafIndex = -1});
    }

    needsRebuild = true;
    return {index, entries[index].generation};
}

void BVHScene::RemoveNode(BVHHandle handle)
{
    if (!IsHandleAlive(handle))
        return;

    auto& entry = entries[handle.index];
    entry.node = {};
    entry.active = false;
    entry.leafIndex = -1;
    entry.generation++;
    freeEntryIndices.push_back(handle.index);
    needsRebuild = true;
}

void BVHScene::MarkDirty(BVHHandle handle)
{
    if (!IsHandleAlive(handle))
        return;

    if (needsRebuild)
        return;

    int leafIndex = entries[handle.index].leafIndex;
    if (leafIndex != -1)
        pendingRefit.insert(static_cast<uint32_t>(leafIndex));
    else
        needsRebuild = true;
}

void BVHScene::Tick()
{
    UpdateHierarchy();

    if (EngineDebugVars::SceneBVH())
    {
        BVHDebug();
    }
}

void BVHScene::UpdateHierarchy()
{
    if (needsRebuild)
    {
        Build(12);
        needsRebuild = false;
    }

    Refit();
}

void BVHScene::ResetRuntimeState()
{
    ClearHierarchy();
    entries.clear();
    freeEntryIndices.clear();
    needsRebuild = false;
}

std::vector<BVHScene::HierarchyNode*> BVHScene::QueryNodesInFrustum(const Frustum& frustum)
{
    std::vector<HierarchyNode*> result{};

    if (!nodes.empty())
    {
        auto& root = GetRoot();
        QueryNodesInFrustum(frustum, root, result);
    }

    return result;
}

std::vector<BVHNode*> BVHScene::QueryRay(const Ray& ray, BVHNodeType type, float maxDistance)
{
    std::vector<BVHNode*> result{};

    UpdateHierarchy();

    if (!nodes.empty())
    {
        auto& root = GetRoot();
        QueryRay(ray, root, type, maxDistance, result);
    }

    return result;
}

std::vector<BVHNode*> BVHScene::QueryInFrustum(const Frustum& frustum, BVHNodeType type)
{
    std::vector<BVHNode*> objs{};
    auto queryNodes = QueryNodesInFrustum(frustum);
    for (auto n : queryNodes)
    {
        for (auto objIdx : n->objectIndices)
        {
            BVHNode* obj = GetNode(objects[objIdx]);
            if (obj && obj->type == type && IsObjectValid(objIdx))
            {
                AABB aabb = obj->getAABB();
                if (HierarchyNode::IsVisibleInFrustum(aabb, frustum))
                    objs.push_back(obj);
            }
        }
    }

    return objs;
}

std::vector<MeshRenderer*> BVHScene::QueryMeshRenderersInFrustum(const Frustum& frustum)
{
    std::vector<MeshRenderer*> renderers{};
    auto nodes = QueryInFrustum(frustum, BVHNodeType::MeshRenderer);
    for (auto node : nodes)
    {
        if (node->owner)
            renderers.push_back(static_cast<MeshRenderer*>(node->owner));
    }
    return renderers;
}

bool BVHScene::IsHandleAlive(BVHHandle handle) const
{
    return handle.index < entries.size() && entries[handle.index].active &&
           entries[handle.index].generation == handle.generation;
}

BVHNode* BVHScene::GetNode(BVHHandle handle)
{
    if (!IsHandleAlive(handle))
        return nullptr;
    return &entries[handle.index].node;
}

AABB BVHScene::GetObjectAABB(int objectIndex)
{
    BVHNode* node = GetNode(objects[objectIndex]);
    if (node && node->getAABB)
        return node->getAABB();
    return {};
}

bool BVHScene::IsObjectValid(int objectIndex) const
{
    const BVHHandle handle = objects[objectIndex];
    if (!IsHandleAlive(handle))
        return false;

    const auto& node = entries[handle.index].node;
    return node.isValid ? node.isValid() : true;
}

void BVHScene::Build(int maxNodeLevel)
{
    ClearHierarchy();

    for (uint32_t entryIndex = 0; entryIndex < entries.size(); ++entryIndex)
    {
        auto& entry = entries[entryIndex];
        entry.leafIndex = -1;
        if (!entry.active)
            continue;
        if (entry.node.isValid && !entry.node.isValid())
            continue;
        if (!entry.node.getAABB)
            continue;

        objects.push_back({entryIndex, entry.generation});
        objectCenters.push_back(entry.node.getAABB().GetCenter());
    }

    if (objects.empty())
        return;

    int totalNodes = (glm::pow(2, maxNodeLevel) - 1);
    nodes.resize(totalNodes);
    maxNonLeafNodeIndex = glm::pow(2, maxNodeLevel - 1) - 2;

    int rootNodeIndex = 0;
    for (int objectIndex = 0; objectIndex < objects.size(); ++objectIndex)
    {
        nodes[rootNodeIndex].objectIndices.push_back(objectIndex);
    }

    for (int i = 0; i < nodes.size(); i++)
    {
        UpdateNode(i);
    }

    for (int i = 0; i < nodes.size(); ++i)
    {
        if (nodes[i].IsLeaf() && !nodes[i].IsEmpty())
        {
            for (int objIdx : nodes[i].objectIndices)
            {
                entries[objects[objIdx].index].leafIndex = i;
            }
        }
    }
}

void BVHScene::ClearHierarchy()
{
    nodes.clear();
    objects.clear();
    objectCenters.clear();
    pendingRefit.clear();
    maxNonLeafNodeIndex = 0;
}

void BVHScene::UpdateNodeBounds(int nodeIndex)
{
    HierarchyNode& node = nodes[nodeIndex];

    if (!node.objectIndices.empty())
    {
        node.aabb.min = glm::float3(std::numeric_limits<float>::max());
        node.aabb.max = glm::float3(std::numeric_limits<float>::lowest());
        for (auto idx : node.objectIndices)
        {
            AABB aabb = GetObjectAABB(idx);
            node.aabb.min = glm::min(node.aabb.min, aabb.min);
            node.aabb.max = glm::max(node.aabb.max, aabb.max);
        }
    }
}

void BVHScene::UpdateNode(int nodeIndex)
{
    HierarchyNode& node = nodes[nodeIndex];

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
        int childIndex = objectCenters[objIdx][longestAxis] < separationPlane ? node.childNodeLeft : node.childNodeRight;
        ASSERT(childIndex < nodes.size());
        nodes[childIndex].objectIndices.push_back(objIdx);
        nodes[childIndex].parentIndex = nodeIndex;
    }
}

void BVHScene::Refit()
{
    for (auto nodeIdx : pendingRefit)
    {
        Refit(static_cast<int>(nodeIdx));
    }
    pendingRefit.clear();
}

void BVHScene::Refit(int nodeIndex)
{
    int curr = nodeIndex;
    while (curr != -1)
    {
        HierarchyNode& node = nodes[curr];
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

void BVHScene::QueryRay(
    const Ray& ray,
    HierarchyNode& node,
    BVHNodeType type,
    float maxDistance,
    std::vector<BVHNode*>& hits
)
{
    float nodeDistance = 0.0f;
    if (!RayVsAABB(ray, node.aabb, nodeDistance) || nodeDistance > maxDistance)
        return;

    if (node.IsLeaf())
    {
        for (auto objIdx : node.objectIndices)
        {
            BVHNode* obj = GetNode(objects[objIdx]);
            if (obj == nullptr || obj->type != type || !IsObjectValid(objIdx) || !obj->getAABB)
                continue;

            float objectDistance = 0.0f;
            if (RayVsAABB(ray, obj->getAABB(), objectDistance) && objectDistance <= maxDistance)
                hits.push_back(obj);
        }
        return;
    }

    if (node.HasLeftChild())
        QueryRay(ray, nodes[node.childNodeLeft], type, maxDistance, hits);

    if (node.HasRightChild())
        QueryRay(ray, nodes[node.childNodeRight], type, maxDistance, hits);
}

void BVHScene::QueryNodesInFrustum(const Frustum& frustum, HierarchyNode& node, std::vector<HierarchyNode*>& inFrustum)
{
    if (node.IsFullyVisibleInFrustum(frustum))
    {
        inFrustum.push_back(&node);
    }
    else if (node.IsVisibleInFrustum(node.aabb, frustum))
    {
        if (node.HasLeftChild())
            QueryNodesInFrustum(frustum, nodes[node.childNodeLeft], inFrustum);

        if (node.HasRightChild())
            QueryNodesInFrustum(frustum, nodes[node.childNodeRight], inFrustum);

        if (node.IsLeaf())
        {
            inFrustum.push_back(&node);
        }
    }
}

void BVHScene::BVHDebug()
{
    static int debugLevel = 2;
    static bool drawObjBounds = false;
    static bool bvhDebug = false;
    static bool frustumCull = false;
    static bool testCullObject = false;

    ImGui::Begin("BVH Debug");
    ImGui::Checkbox("Bvh Debug", &bvhDebug);
    ImGui::Checkbox("Frustum Cull", &frustumCull);
    ImGui::Checkbox("Draw Object Bounds", &drawObjBounds);
    ImGui::Checkbox("Test Cull Object", &testCullObject);
    ImGui::InputInt("Debug Level", &debugLevel);
    ImGui::End();

    if (scene == nullptr || scene->GetMainCamera() == nullptr)
        return;

    Mesh* mesh = GetBVHDebugMesh();
    Material& mat = GetBVHDebugMaterial();
    Frustum frustum = scene->GetMainCamera()->GetFrustum();

    auto config = *mat.GetShaderProgram()->GetDefaultShaderConfig();
    config.polygonMode = Gfx::PolygonMode::Line;
    mat.SetShaderConfig(config);

    auto drawAABB = [&](const AABB& aabb)
    {
        glm::float3 scale = (aabb.max - aabb.min);
        if (scale.x != 0 && scale.y != 0 && scale.z != 0)
        {
            glm::float3 position = (aabb.max + aabb.min) / 2.0f;
            glm::float4x4 model = glm::translate(glm::mat4(1), position) * glm::scale(glm::mat4(1), scale);
            Graphics::DrawMesh(*mesh, 0, model, mat);
        }
    };

    if (testCullObject)
    {
        auto queryNodes = QueryNodesInFrustum(frustum);
        for (auto n : queryNodes)
        {
            for (auto objIdx : n->objectIndices)
            {
                BVHNode* obj = GetNode(objects[objIdx]);
                if (obj && IsObjectValid(objIdx))
                {
                    AABB aabb = obj->getAABB();
                    if (HierarchyNode::IsVisibleInFrustum(aabb, frustum))
                        drawAABB(aabb);
                }
            }
        }
    }
    else if (bvhDebug)
    {
        if (debugLevel < 0)
        {
            for (auto& n : nodes)
            {
                if (n.IsLeaf() && !n.IsEmpty())
                {
                    if (frustumCull && !n.IsVisibleInFrustum(n.aabb, frustum))
                        continue;

                    drawAABB(n.aabb);

                    if (drawObjBounds)
                    {
                        for (auto objIdx : n.objectIndices)
                        {
                            BVHNode* obj = GetNode(objects[objIdx]);
                            if (obj && IsObjectValid(objIdx))
                                drawAABB(obj->getAABB());
                        }
                    }
                }
            }
        }
        else
        {
            for (int i = glm::pow(2, debugLevel) - 1; i < glm::pow(2, debugLevel + 1) - 1; ++i)
            {
                if (i >= nodes.size())
                    return;
                auto& n = nodes[i];
                if (!n.IsEmpty())
                {
                    if (frustumCull && !n.IsVisibleInFrustum(n.aabb, frustum))
                        continue;

                    drawAABB(n.aabb);

                    if (drawObjBounds)
                    {
                        for (auto objIdx : n.objectIndices)
                        {
                            BVHNode* obj = GetNode(objects[objIdx]);
                            if (obj && IsObjectValid(objIdx))
                                drawAABB(obj->getAABB());
                        }
                    }
                }
            }
        }
    }
}

Mesh* BVHScene::GetBVHDebugMesh()
{
    if (bvhDebugMesh == nullptr)
    {
        bvhDebugMesh = EngineInternalResources::GetModels().cube;
    }
    return bvhDebugMesh;
}

Material& BVHScene::GetBVHDebugMaterial()
{
    if (bvhDebugMaterial == nullptr)
    {
        bvhDebugMaterial = std::make_unique<Material>(ShaderLibrary::GetShader(Shaders::SimpleColor));
    }
    return *bvhDebugMaterial;
}

bool BVHScene::HierarchyNode::IsFullyVisibleInFrustum(const Frustum& frustum)
{
    const glm::vec3& vmin = aabb.min;
    const glm::vec3& vmax = aabb.max;

    for (size_t i = 0; i < 6; ++i)
    {
        const glm::vec4& g = frustum.planes[i];
        if ((glm::dot(g, glm::vec4(vmin.x, vmin.y, vmin.z, 1.0f)) < 0.0) ||
            (glm::dot(g, glm::vec4(vmax.x, vmin.y, vmin.z, 1.0f)) < 0.0) ||
            (glm::dot(g, glm::vec4(vmin.x, vmax.y, vmin.z, 1.0f)) < 0.0) ||
            (glm::dot(g, glm::vec4(vmax.x, vmax.y, vmin.z, 1.0f)) < 0.0) ||
            (glm::dot(g, glm::vec4(vmin.x, vmin.y, vmax.z, 1.0f)) < 0.0) ||
            (glm::dot(g, glm::vec4(vmax.x, vmin.y, vmax.z, 1.0f)) < 0.0) ||
            (glm::dot(g, glm::vec4(vmin.x, vmax.y, vmax.z, 1.0f)) < 0.0) ||
            (glm::dot(g, glm::vec4(vmax.x, vmax.y, vmax.z, 1.0f)) < 0.0))
        {
            return false;
        }
    }

    return true;
}

bool BVHScene::HierarchyNode::IsVisibleInFrustum(const AABB& aabb, const Frustum& frustum)
{
    const glm::vec3& vmin = aabb.min;
    const glm::vec3& vmax = aabb.max;

    for (size_t i = 0; i < 6; ++i)
    {
        const glm::vec4& g = frustum.planes[i];
        if ((glm::dot(g, glm::vec4(vmin.x, vmin.y, vmin.z, 1.0f)) < 0.0) &&
            (glm::dot(g, glm::vec4(vmax.x, vmin.y, vmin.z, 1.0f)) < 0.0) &&
            (glm::dot(g, glm::vec4(vmin.x, vmax.y, vmin.z, 1.0f)) < 0.0) &&
            (glm::dot(g, glm::vec4(vmax.x, vmax.y, vmin.z, 1.0f)) < 0.0) &&
            (glm::dot(g, glm::vec4(vmin.x, vmin.y, vmax.z, 1.0f)) < 0.0) &&
            (glm::dot(g, glm::vec4(vmax.x, vmin.y, vmax.z, 1.0f)) < 0.0) &&
            (glm::dot(g, glm::vec4(vmin.x, vmax.y, vmax.z, 1.0f)) < 0.0) &&
            (glm::dot(g, glm::vec4(vmax.x, vmax.y, vmax.z, 1.0f)) < 0.0))
        {
            return false;
        }
    }

    return true;
}
