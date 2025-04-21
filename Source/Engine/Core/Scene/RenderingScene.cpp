#include "RenderingScene.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "Core/EngineInternalResources.hpp"
#include "Libs/Math.hpp"
#include "Rendering/Graphics.hpp"

#include "ThirdParty/imgui/imgui.h"

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

std::vector<BoundingVolumeHierarchy::Node*> BoundingVolumeHierarchy::QueryNodesInFrustum(float4 cameraPlanes[6])
{
    std::vector<Node*> resultNodes;
    for (auto& node : nodes)
    {
        if (node.IsLeaf() && !node.IsEmpty())
        {
            bool isInFrustum = true;
            for (int i = 0; i < 6; ++i)
            {
                if (glm::dot(cameraPlanes[i], glm::float4(node.aabb.min, 1)) > 0 &&
                    glm::dot(cameraPlanes[i], glm::float4(node.aabb.max, 1)) > 0)
                {
                    isInFrustum = false;
                    break;
                }
            }
            if (isInFrustum)
            {
                resultNodes.push_back(&node);
            }
        }
    }
    return resultNodes;
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
    // BVHDebug()
}

void RenderingScene::BVHDebug()
{
    static int debugLevel = 2;
    static bool drawObjBounds = false;
    static bool bvhDebug = false;
    ImGui::Begin("BVH Debug");
    ImGui::Checkbox("Bvh Debug", &bvhDebug);
    ImGui::InputInt("Debug Level", &debugLevel);
    ImGui::End();
    static Mesh* mesh = EngineInternalResources::GetModels().cube;
    static Material mat = Material(ShaderLibrary::GetShader(ShaderLibrary::SimpleColor));
    if (bvhDebug)
    {
        auto config = *mat.GetShaderProgram()->GetDefaultShaderConfig();
        config.polygonMode = Gfx::PolygonMode::Line;
        mat.SetShaderConfig(config);
        if (debugLevel < 0)
        {
            for (auto& n : rendererNodeHierarchy.nodes)
            {
                if (n.IsLeaf() && !n.IsEmpty())
                {
                    glm::float3 scale = (n.aabb.max - n.aabb.min);
                    if (scale.x != 0 && scale.y != 0 && scale.z != 0)
                    {
                        glm::float3 position = (n.aabb.max + n.aabb.min) / 2.0f;
                        glm::float4x4 model = glm::translate(glm::mat4(1), position) * glm::scale(glm::mat4(1), scale);
                        Graphics::DrawMesh(*mesh, 0, model, mat);
                    }

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
        else
        {
            for (int i = glm::pow(2, debugLevel) - 1; i < glm::pow(2, debugLevel + 1) - 1; ++i)
            {
                if (i >= rendererNodeHierarchy.nodes.size())
                    return;
                auto& n = rendererNodeHierarchy.nodes[i];
                if (!n.IsEmpty())
                {
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
