#include "RenderingScene.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "Core/EngineInternalResources.hpp"
#include "Libs/Math.hpp"
#include "Rendering/Graphics.hpp"

#include "ThirdParty/imgui/imgui.h"

void RenderingScene::BoundingVolumeHierarchy::Build(MeshRenderer** bvhObjects, int objectsCount, int maxNodeLevel)
{
    int totalNodes = (glm::pow(2, maxNodeLevel) - 1);
    this->nodes.resize(totalNodes);
    maxNonLeafNodeIndex = (totalNodes - 3) / 2;

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

void RenderingScene::BoundingVolumeHierarchy::UpdateNodeBounds(int nodeIndex)
{
    Node& node = nodes[nodeIndex];
    node.aabb.min = glm::float3(std::numeric_limits<float>::max());
    node.aabb.max = glm::float3(std::numeric_limits<float>::lowest());
    for (auto idx : node.objectIndices)
    {
        node.aabb.min = glm::min(node.aabb.min, objects[idx]->GetAABB().min);
        node.aabb.max = glm::max(node.aabb.max, objects[idx]->GetAABB().max);
    }
}

void RenderingScene::BoundingVolumeHierarchy::UpdateNode(int nodeIndex)
{
    Node& node = nodes[nodeIndex];

    UpdateNodeBounds(nodeIndex);

    if (nodeIndex > maxNonLeafNodeIndex || node.objectIndices.size() <= 4)
    {
        return;
    }

    node.childNodeLeft = nodeIndex * 2 + 1;
    node.childNodeRight = nodeIndex * 2 + 2;

    glm::float3 extent = node.aabb.max - node.aabb.min;
    int longestAxis = 0;
    if (extent.y > extent.x)
        longestAxis = 1;
    if (extent.z > extent.y)
        longestAxis = 2;

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
        rendererNodeHierarchy.Build(meshRenderers.data(), meshRenderers.size(), 8);
        updateRendererNodeHierarchy = false;
    }

    static int debugLevel = 2;
    ImGui::Begin("BVH Debug");
    ImGui::DragInt("debug level", &debugLevel);
    ImGui::End();
    static Mesh* mesh = EngineInternalResources::GetModels().cube;
    static Material mat = Material(ShaderLibrary::GetShader(ShaderLibrary::SimpleForwardLit));
    auto config = *mat.GetShaderProgram()->GetDefaultShaderConfig();
    config.polygonMode = Gfx::PolygonMode::Line;
    mat.SetShaderConfig(config);
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

            for (auto objIdx : n.objectIndices)
            {
                auto obj = rendererNodeHierarchy.objects[objIdx].Get();
                if (obj)
                {
                    auto aabb = rendererNodeHierarchy.objects[objIdx]->GetAABB();

                    glm::float3 position = (aabb.max + aabb.min) / 2.0f;
                    glm::float3 scale = (aabb.max - aabb.min);
                    glm::float4x4 model = glm::translate(glm::mat4(1), position) * glm::scale(glm::mat4(1), scale);
                    Graphics::DrawMesh(*mesh, 0, model, mat);
                }
            }
        }
    }
}
