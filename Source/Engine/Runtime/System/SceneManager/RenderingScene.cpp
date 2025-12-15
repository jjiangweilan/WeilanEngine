#include "RenderingScene.hpp"
#include "Engine/Runtime/Object/Component/MeshRenderer.hpp"
#include "Engine/Runtime/Object/Component/SceneEnvironment.hpp"
#include "Engine/MiddleLayer/EngineDebug.hpp"
#include "Engine/MiddleLayer/EngineInternalResources.hpp"
#include "Engine/Runtime/System/SceneManager/Scene.hpp"
#include "Engine/Library/Math.hpp"
#include "Engine/Runtime/System/Rendering/Graphics.hpp"

#include "Engine/ThirdParty/imgui/imgui.h"

SceneEnvironmentData& RenderingScene::GetSceneEnvironmentData()
{
    static SceneEnvironmentData defaultData;
    if (sceneEnvironment)
    {
        return sceneEnvironment->data;
    }
    return defaultData;
}

void BoundingVolumeHierarchy::Build(MeshRenderer** bvhObjects, int objectsCount, int maxNodeLevel)
{
    nodes.clear();
    objects.clear();
    objectCenters.clear();

    int totalNodes = (glm::pow(2, maxNodeLevel) - 1);
    this->nodes.resize(totalNodes);
    maxNonLeafNodeIndex = glm::pow(2, maxNodeLevel - 1) - 2;

    int rootNodeIndex = 0;
    for (int objectIndex = 0; objectIndex < objectsCount; ++objectIndex)
    {
        nodes[rootNodeIndex].objectIndices.push_back(objectIndex);
        objects.push_back(bvhObjects[objectIndex]);
        objectCenters.push_back(bvhObjects[objectIndex]->GetAABB().GetCenter());
    }

    for (int i = 0; i < nodes.size(); i++)
    {
        UpdateNode(i);
    }
}

void BoundingVolumeHierarchy::UpdateNodeBounds(int nodeIndex)
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

std::vector<BoundingVolumeHierarchy::Node*> BoundingVolumeHierarchy::QueryNodesInFrustum(const Frustum& Frustum)
{
    std::vector<BoundingVolumeHierarchy::Node*> result{};

    if (!nodes.empty())
    {
        auto& root = GetRoot();
        QueryNodesInFrustum(Frustum, root, result);
    }

    return result;
}

void BoundingVolumeHierarchy::QueryNodesInFrustum(
    const Frustum& Frustum, Node& node, std::vector<BoundingVolumeHierarchy::Node*>& inFrustum
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

void BoundingVolumeHierarchy::UpdateNode(int nodeIndex)
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
    }
}

void RenderingScene::Tick()
{
    for (auto m : meshRenderers)
    {
        m->UpdateSkinning();
    }

    if (updateRendererNodeHierarchy)
    {
        rendererNodeHierarchy.Build(meshRenderers.data(), meshRenderers.size(), 12);
        updateRendererNodeHierarchy = false;
    }

    // BVH Debug
    if (EngineDebugVars::SceneBVH())
    {
        BVHDebug();
    }
}

void RenderingScene::BVHDebug()
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
    static Mesh* mesh = EngineInternalResources::GetModels().cube;
    static Material mat = Material(ShaderLibrary::GetShader(Shaders::SimpleColor));
    Frustum frustum = scene->GetMainCamera()->GetFrustum();

    auto config = *mat.GetShaderProgram()->GetDefaultShaderConfig();
    config.polygonMode = Gfx::PolygonMode::Line;
    mat.SetShaderConfig(config);

    if (testCullObject)
    {
        auto nodes = rendererNodeHierarchy.QueryNodesInFrustum(frustum);
        for (auto n : nodes)
        {
            for (auto objIdx : n->objectIndices)
            {
                auto obj = rendererNodeHierarchy.objects[objIdx].Get();
                if (obj)
                {
                    auto aabb = rendererNodeHierarchy.objects[objIdx]->GetAABB();

                    if (BoundingVolumeHierarchy::Node::IsVisibleInFrustum(aabb, frustum))
                    {
                        glm::float3 position = (aabb.max + aabb.min) / 2.0f;
                        glm::float3 scale = (aabb.max - aabb.min);
                        glm::float4x4 model = glm::translate(glm::mat4(1), position) * glm::scale(glm::mat4(1), scale);
                        Graphics::DrawMesh(*mesh, 0, model, mat);
                    }
                }
            }
        }
    }
    else if (bvhDebug)
    {
        if (debugLevel < 0)
        {
            for (auto& n : rendererNodeHierarchy.nodes)
            {
                if (n.IsLeaf() && !n.IsEmpty())
                {
                    if (frustumCull)
                    {
                        if (!n.IsVisibleInFrustum(n.aabb, frustum))
                        {
                            continue;
                        }
                    }

                    glm::float3 scale = (n.aabb.max - n.aabb.min);
                    if (scale.x != 0 && scale.y != 0 && scale.z != 0)
                    {
                        glm::float3 position = (n.aabb.max + n.aabb.min) / 2.0f;
                        glm::float4x4 model = glm::translate(glm::mat4(1), position) * glm::scale(glm::mat4(1), scale);
                        Graphics::DrawMesh(*mesh, 0, model, mat);
                    }

                    if (drawObjBounds)
                    {
                        for (auto objIdx : n.objectIndices)
                        {
                            auto obj = rendererNodeHierarchy.objects[objIdx].Get();
                            if (obj)
                            {
                                auto aabb = rendererNodeHierarchy.objects[objIdx]->GetAABB();

                                glm::float3 position = (aabb.max + aabb.min) / 2.0f;
                                glm::float3 scale = (aabb.max - aabb.min);
                                glm::float4x4 model =
                                    glm::translate(glm::mat4(1), position) * glm::scale(glm::mat4(1), scale);
                                Graphics::DrawMesh(*mesh, 0, model, mat);
                            }
                        }
                    }
                }
            }
        }
        else
        {
            for (int i = glm::pow(2, debugLevel) - 1; i < glm::pow(2, debugLevel + 1) - 1; ++i)
            {
                if (i >= rendererNodeHierarchy.nodes.size())
                    return;
                auto& n = rendererNodeHierarchy.nodes[i];
                if (!n.IsEmpty())
                {
                    if (frustumCull)
                    {
                        if (!n.IsVisibleInFrustum(n.aabb, frustum))
                        {
                            continue;
                        }
                    }

                    glm::float3 scale = (n.aabb.max - n.aabb.min);
                    if (scale.x != 0 && scale.y != 0 && scale.z != 0)
                    {
                        glm::float3 position = (n.aabb.max + n.aabb.min) / 2.0f;
                        glm::float4x4 model = glm::translate(glm::mat4(1), position) * glm::scale(glm::mat4(1), scale);
                        Graphics::DrawMesh(*mesh, 0, model, mat);
                    }

                    if (drawObjBounds)
                    {
                        for (auto objIdx : n.objectIndices)
                        {
                            auto obj = rendererNodeHierarchy.objects[objIdx].Get();
                            if (obj)
                            {
                                auto aabb = rendererNodeHierarchy.objects[objIdx]->GetAABB();

                                glm::float3 position = (aabb.max + aabb.min) / 2.0f;
                                glm::float3 scale = (aabb.max - aabb.min);
                                glm::float4x4 model =
                                    glm::translate(glm::mat4(1), position) * glm::scale(glm::mat4(1), scale);
                                Graphics::DrawMesh(*mesh, 0, model, mat);
                            }
                        }
                    }
                }
            }
        }
    }
}
bool BoundingVolumeHierarchy::Node::IsFullyVisibleInFrustum(const Frustum& Frustum)
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

bool BoundingVolumeHierarchy::Node::IsVisibleInFrustum(const AABB& aabb, const Frustum& Frustum)
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

std::vector<MeshRenderer*> BoundingVolumeHierarchy::QueryRendererInFrustum(const Frustum& frustum)
{
    std::vector<MeshRenderer*> objs{};
    auto nodes = QueryNodesInFrustum(frustum);
    for (auto n : nodes)
    {
        for (auto objIdx : n->objectIndices)
        {
            auto obj = objects[objIdx].Get();
            if (obj && Node::IsVisibleInFrustum(obj->GetAABB(), frustum))
                objs.push_back(obj);
        }
    }

    return objs;
}
